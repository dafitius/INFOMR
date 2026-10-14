#include "statistics.h"

#include "../io/shape_db.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QRandomGenerator>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QThread>
#include <QVBoxLayout>
#include <algorithm>
#include <set>

namespace
{
	std::filesystem::path toPath(const QString& path)
	{
		return std::filesystem::path(path.toStdU16String());
	}
} // namespace

Statistics::Statistics(std::shared_ptr<ShapeDb> database, QWidget* parent)
	: QWidget(parent)
	, m_database(std::move(database))
	, m_cancel(std::make_shared<std::atomic<bool>>(false))
{
	setupUI();
}

Statistics::~Statistics()
{
	if (m_worker != nullptr)
	{
		m_cancel->store(true);
		m_worker->wait();
	}
}

void Statistics::setupUI()
{
	auto* root = new QVBoxLayout(this);

	auto* top = new QHBoxLayout;
	top->setContentsMargins(6, 6, 6, 0);
	m_computeButton = new QPushButton(tr("Compute"), this);
	m_computeButton->setToolTip(tr("Load the shapes from the database and compute their statistics"));
	m_limitCheck = new QCheckBox(tr("Limit to"), this);
	m_limitCheck->setToolTip(tr("Only use a random subset, spread evenly over the classes"));
	m_limitSpin = new QSpinBox(this);
	m_limitSpin->setRange(1, 5000);
	m_limitSpin->setValue(1000);
	m_limitSpin->setSuffix(tr(" models"));
	m_limitSpin->setEnabled(false);
	m_loadButton = new QPushButton(tr("Load CSV"), this);
	m_exportButton = new QPushButton(tr("Export CSV"), this);
	m_exportButton->setEnabled(false);
	m_progress = new QProgressBar(this);
	m_progress->setVisible(false);
	m_statusLabel = new QLabel(this);

	top->addWidget(m_computeButton);
	top->addWidget(m_limitCheck);
	top->addWidget(m_limitSpin);
	top->addSpacing(12);
	top->addWidget(m_loadButton);
	top->addWidget(m_exportButton);
	top->addSpacing(12);
	top->addWidget(m_progress, 1);
	top->addWidget(m_statusLabel);
	top->addStretch();
	root->addLayout(top);

	m_splitter = new QSplitter(Qt::Horizontal, this);

	auto* left = new QWidget(m_splitter);
	auto* leftLayout = new QVBoxLayout(left);
	leftLayout->setContentsMargins(0, 0, 0, 0);
	m_classFilter = new QComboBox(left);
	m_classFilter->setToolTip(tr("Only show the shapes of this class in the histograms"));
	m_list = new QListWidget(left);
	m_list->setMinimumWidth(120);
	leftLayout->addWidget(m_classFilter);
	leftLayout->addWidget(m_list, 1);

	auto* right = new QWidget(m_splitter);
	auto* rightLayout = new QVBoxLayout(right);
	auto* sliderRow = new QHBoxLayout;
	sliderRow->addWidget(new QLabel(tr("Buckets:"), right));
	m_bucketSlider = new QSlider(Qt::Horizontal, right);
	m_bucketSlider->setRange(1, 200);
	m_bucketSlider->setValue(20);
	m_bucketLabel = new QLabel(right);
	m_bucketLabel->setMinimumWidth(160);
	sliderRow->addWidget(m_bucketSlider, 1);
	sliderRow->addWidget(m_bucketLabel);
	m_view = new HistogramView(right);
	m_summaryLabel = new QLabel(right);
	m_summaryLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
	rightLayout->addLayout(sliderRow);
	rightLayout->addWidget(m_view, 1);
	rightLayout->addWidget(m_summaryLabel);

	m_splitter->addWidget(left);
	m_splitter->addWidget(right);
	m_splitter->setStretchFactor(0, 0);
	m_splitter->setStretchFactor(1, 1);
	m_splitter->setSizes({220, 800});
	m_splitter->setChildrenCollapsible(false);
	root->addWidget(m_splitter, 1);

	auto updateBucketCount = [this] {
		m_bucketLabel->setText(tr("%1").arg(m_view->bucketCount()));
	};
	connect(m_bucketSlider, &QSlider::valueChanged, this, [this, updateBucketCount](int v) {
		m_view->setBucketCount(v);
		updateBucketCount();
	});
	connect(m_limitCheck, &QCheckBox::toggled, m_limitSpin, &QWidget::setEnabled);
	connect(m_computeButton, &QPushButton::clicked, this, &Statistics::compute);
	connect(m_loadButton, &QPushButton::clicked, this, &Statistics::loadCsv);
	connect(m_exportButton, &QPushButton::clicked, this, &Statistics::exportCsv);
	connect(m_classFilter, &QComboBox::currentIndexChanged, this, [this](int) { rebuildHistograms(); });
	connect(m_list, &QListWidget::currentRowChanged, this, [this, updateBucketCount](int) {
		onSelectionChanged();
		updateBucketCount();
	});
	connect(m_view, &HistogramView::bucketClicked, this, &Statistics::onBucketClicked);
	updateBucketCount();
}

void Statistics::setBusy(const bool busy)
{
	m_computeButton->setEnabled(!busy);
	m_loadButton->setEnabled(!busy);
	m_exportButton->setEnabled(!busy && !m_shapes.empty());
	m_progress->setVisible(busy);
}

void Statistics::compute()
{
	if (m_worker != nullptr)
	{
		return;
	}

	const size_t limit = m_limitCheck->isChecked() ? static_cast<size_t>(m_limitSpin->value()) : 0;
	auto shapes = m_database->randomClassBalancedShapes(limit);

	setBusy(true);
	m_progress->setRange(0, static_cast<int>(shapes.size()));
	m_progress->setValue(0);
	m_statusLabel->setText(tr("Computing statistics of %1 shapes...").arg(shapes.size()));
	m_cancel->store(false);

	m_worker = QThread::create(
		[this, database = m_database, cancel = m_cancel, shapes = std::move(shapes), progress = m_progress]
		{
			auto onProgress = [progress](const size_t done, size_t)
			{
				QMetaObject::invokeMethod(progress, [progress, done] { progress->setValue(std::max(progress->value(), static_cast<int>(done))); }, Qt::QueuedConnection);
			};
			auto result = std::make_shared<stats::DatabaseStats>(stats::compute(*database, shapes, cancel.get(), onProgress));

			QMetaObject::invokeMethod(
				this,
				[this, result]
				{
					for (const auto& path : result->failed)
					{
						qWarning() << "Failed to load mesh:" << QString::fromStdString(path.string());
					}
					setShapes(std::move(result->shapes));
					m_statusLabel->setText(
						result->failed.empty() ? tr("%1 shapes").arg(m_shapes.size())
											   : tr("%1 shapes, %2 failed to load").arg(m_shapes.size()).arg(result->failed.size()));
				},
				Qt::QueuedConnection);
		});
	m_worker->setParent(this);
	connect(m_worker, &QThread::finished, this, [this] {
		m_worker->deleteLater();
		m_worker = nullptr;
		setBusy(false);
	});
	m_worker->start();
}

void Statistics::loadCsv()
{
	const QString path = QFileDialog::getOpenFileName(this, tr("Load statistics"), QString(), tr("CSV files (*.csv);;All files (*)"));
	if (path.isEmpty())
	{
		return;
	}

	auto shapes = stats::readCsv(toPath(path));
	if (!shapes)
	{
		m_statusLabel->setText(tr("Could not read %1").arg(QFileInfo(path).fileName()));
		return;
	}
	setShapes(std::move(*shapes));
	m_statusLabel->setText(tr("%1 shapes from %2").arg(m_shapes.size()).arg(QFileInfo(path).fileName()));
	setBusy(false);
}

void Statistics::exportCsv()
{
	const QString path = QFileDialog::getSaveFileName(this, tr("Export statistics"), "shape_db_statistics.csv", tr("CSV files (*.csv)"));
	if (path.isEmpty())
	{
		return;
	}

	const bool ok = stats::writeCsv(toPath(path), m_shapes);
	m_statusLabel->setText(ok ? tr("Exported %1 shapes to %2").arg(m_shapes.size()).arg(QFileInfo(path).fileName())
							  : tr("Could not write %1").arg(QFileInfo(path).fileName()));
}

void Statistics::setShapes(std::vector<stats::ShapeStats> shapes)
{
	m_shapes = std::move(shapes);
	m_exportButton->setEnabled(!m_shapes.empty());
	updateClassFilter();
	rebuildHistograms();
}

void Statistics::updateClassFilter()
{
	const QString previous = m_classFilter->currentData().toString();

	std::set<std::string> classNames;
	for (const auto& shape : m_shapes)
	{
		classNames.insert(shape.className);
	}

	m_classFilter->blockSignals(true);
	m_classFilter->clear();
	m_classFilter->addItem(tr("All classes"), QString());
	for (const auto& className : classNames)
	{
		const QString name = QString::fromStdString(className);
		m_classFilter->addItem(name, name);
	}
	m_classFilter->setCurrentIndex(std::max(0, m_classFilter->findData(previous)));
	m_classFilter->blockSignals(false);
}

void Statistics::rebuildHistograms()
{
	const QString previous = (m_list->currentItem() != nullptr) ? m_list->currentItem()->text() : QString();
	const std::string classFilter = m_classFilter->currentData().toString().toStdString();

	std::vector<const stats::ShapeStats*> rows;
	for (const auto& shape : m_shapes)
	{
		if (classFilter.empty() || shape.className == classFilter)
		{
			rows.push_back(&shape);
		}
	}

	m_view->setHistogram(nullptr);
	m_histograms.clear();
	for (const auto& column : stats::columns())
	{
		QVector<Sample> samples;
		samples.reserve(static_cast<qsizetype>(rows.size()));
		for (const auto* row : rows)
		{
			samples.push_back(Sample(column.value(*row), m_database->resourcePath(row->className, row->fileName)));
		}
		m_histograms.push_back(Histogram{QString::fromUtf8(column.name.data(), static_cast<qsizetype>(column.name.size())),
										 Histogram::defaultBucketCount(rows.size()), samples});
	}

	m_list->blockSignals(true);
	m_list->clear();
	for (const auto& hist : m_histograms)
	{
		m_list->addItem(hist.name);
	}
	m_list->blockSignals(false);

	const auto found = m_list->findItems(previous, Qt::MatchExactly);
	m_list->setCurrentRow(found.isEmpty() ? (m_histograms.isEmpty() ? -1 : 0) : m_list->row(found.front()));
	onSelectionChanged();
}

void Statistics::onSelectionChanged()
{
	int const row = m_list->currentRow();
	if (row < 0 || row >= m_histograms.size() || m_histograms[row].samples.isEmpty())
	{
		m_view->setHistogram(nullptr);
		m_summaryLabel->clear();
		return;
	}

	const auto& histogram = m_histograms[row];
	m_view->setHistogram(&histogram);
	m_bucketSlider->setValue(static_cast<int>(histogram.bucket_size));

	const auto summary = histogram.summary();
	m_summaryLabel->setText(tr("n = %1    min = %2    max = %3    mean = %4    median = %5    std = %6")
								.arg(summary.count)
								.arg(summary.min, 0, 'g', 6)
								.arg(summary.max, 0, 'g', 6)
								.arg(summary.mean, 0, 'g', 6)
								.arg(summary.median, 0, 'g', 6)
								.arg(summary.stddev, 0, 'g', 6));
}

void Statistics::onBucketClicked(const QVector<Sample>& samples)
{
	if (samples.isEmpty()) return;
	const Sample& pick = samples[QRandomGenerator::global()->bounded(samples.size())];
	emit openModel(pick.source);
}
