#include "statistics.h"

#include "../components/histogram.h"
#include "../geometry/pipeline.hpp"
#include "../io/shape_db.hpp"
#include "../stats/compute.hpp"
#include "../stats/distributions.hpp"
#include "../stats/metrics.hpp"
#include "../stats/stats_table.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMetaObject>
#include <QObject>
#include <QProgressBar>
#include <QPushButton>
#include <QRandomGenerator>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QString>
#include <QThread>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>
#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
	std::filesystem::path toPath(const QString& path)
	{
		return std::filesystem::path(path.toStdU16String());
	}

	QString toQString(const std::string_view text)
	{
		return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
	}

	QString summaryText(const Histogram& histogram)
	{
		const auto summary = histogram.summary();
		return QObject::tr("n = %1    min = %2    max = %3    mean = %4    median = %5    std = %6")
			.arg(summary.count)
			.arg(summary.min, 0, 'g', 6)
			.arg(summary.max, 0, 'g', 6)
			.arg(summary.mean, 0, 'g', 6)
			.arg(summary.median, 0, 'g', 6)
			.arg(summary.stddev, 0, 'g', 6);
	}

	std::optional<mesh::Pipeline> pipelineFromRunLabel(std::string label)
	{
		if (auto pipeline = mesh::Pipeline::parse(label))
		{
			return pipeline;
		}

		if (label.empty() || label.back() != ')')
		{
			return std::nullopt;
		}
		const auto suffix = label.rfind(" (");
		if (suffix == std::string::npos || suffix + 2 == label.size() - 1)
		{
			return std::nullopt;
		}
		const auto number = std::string_view(label).substr(suffix + 2, label.size() - suffix - 3);
		if (!std::ranges::all_of(number, [](const unsigned char character) { return std::isdigit(character) != 0; }))
		{
			return std::nullopt;
		}
		return mesh::Pipeline::parse(std::string_view(label).substr(0, suffix));
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

	auto* pipelineRow = new QHBoxLayout;
	pipelineRow->setContentsMargins(6, 6, 6, 0);
	m_pipelineEdit = new QLineEdit(this);
	m_pipelineEdit->setPlaceholderText(tr("raw (no operations)"));
	m_pipelineEdit->setToolTip(tr("Operations applied to every shape before measuring, in order, separated by '>'"));
	m_operationBox = new QComboBox(this);
	for (const auto& operation : mesh::operations())
	{
		m_operationBox->addItem(toQString(operation.name), toQString(operation.name));
		m_operationBox->setItemData(m_operationBox->count() - 1, toQString(operation.description), Qt::ToolTipRole);
	}
	auto* addStepButton = new QPushButton(tr("Add step"), this);
	auto* clearStepsButton = new QPushButton(tr("Clear"), this);
	pipelineRow->addWidget(new QLabel(tr("Pipeline:"), this));
	pipelineRow->addWidget(m_pipelineEdit, 1);
	pipelineRow->addWidget(m_operationBox);
	pipelineRow->addWidget(addStepButton);
	pipelineRow->addWidget(clearStepsButton);
	root->addLayout(pipelineRow);

	auto* top = new QHBoxLayout;
	top->setContentsMargins(6, 0, 6, 0);
	m_computeButton = new QPushButton(tr("Compute"), this);
	m_computeButton->setToolTip(tr("Load the shapes, apply the pipeline and compute their statistics as a new run"));
	m_limitCheck = new QCheckBox(tr("Limit to"), this);
	m_limitCheck->setToolTip(tr("Only use a random subset, spread evenly over the classes"));
	m_limitSpin = new QSpinBox(this);
	m_limitSpin->setRange(1, 5000);
	m_limitSpin->setValue(1000);
	m_limitSpin->setSuffix(tr(" models"));
	m_limitSpin->setEnabled(false);
	m_sameShapesCheck = new QCheckBox(tr("Same shapes as selected run"), this);
	m_sameShapesCheck->setToolTip(tr("Use exactly the shapes of the selected run, so the runs can be compared shape by shape"));
	m_loadButton = new QPushButton(tr("Load CSV"), this);
	m_exportButton = new QPushButton(tr("Export CSV"), this);
	m_exportButton->setToolTip(tr("Export the selected run"));
	m_exportButton->setEnabled(false);
	m_descriptorExportButton = new QPushButton(tr("Export descriptor histograms"), this);
	m_descriptorExportButton->setToolTip(tr("Export a binned A3/D1-D4 histogram for each shape in the selected run and class filter"));
	m_descriptorExportButton->setEnabled(false);
	m_descriptorBins = new QSpinBox(this);
	m_descriptorBins->setRange(1, 1000);
	m_descriptorBins->setValue(100);
	m_descriptorBins->setPrefix(tr("Bins: "));
	m_descriptorBins->setToolTip(tr("Number of bins in each per-shape descriptor histogram"));
	m_progress = new QProgressBar(this);
	m_progress->setVisible(false);
	m_statusLabel = new QLabel(this);

	top->addWidget(m_computeButton);
	top->addWidget(m_limitCheck);
	top->addWidget(m_limitSpin);
	top->addWidget(m_sameShapesCheck);
	top->addSpacing(12);
	top->addWidget(m_loadButton);
	top->addWidget(m_exportButton);
	top->addWidget(m_descriptorBins);
	top->addWidget(m_descriptorExportButton);
	top->addSpacing(12);
	top->addWidget(m_progress, 1);
	top->addWidget(m_statusLabel);
	top->addStretch();
	root->addLayout(top);

	m_splitter = new QSplitter(Qt::Horizontal, this);

	auto* left = new QWidget(m_splitter);
	auto* leftLayout = new QVBoxLayout(left);
	leftLayout->setContentsMargins(0, 0, 0, 0);
	m_runList = new QListWidget(left);
	m_runList->setToolTip(tr("Computed or loaded runs, the selected run is shown in the histogram"));
	m_runList->setMaximumHeight(140);
	m_removeRunButton = new QPushButton(tr("Remove run"), left);
	m_removeRunButton->setEnabled(false);
	m_classFilter = new QComboBox(left);
	m_classFilter->setToolTip(tr("Only show the shapes of this class in the histograms"));
	m_list = new QListWidget(left);
	m_list->setMinimumWidth(120);
	leftLayout->addWidget(new QLabel(tr("Runs"), left));
	leftLayout->addWidget(m_runList);
	leftLayout->addWidget(m_removeRunButton);
	leftLayout->addWidget(m_classFilter);
	leftLayout->addWidget(m_list, 1);

	auto* right = new QWidget(m_splitter);
	auto* rightLayout = new QVBoxLayout(right);
	auto* compareRow = new QHBoxLayout;
	m_compareBox = new QComboBox(right);
	m_compareBox->setToolTip(tr("Second run to compare the selected run with"));
	m_modeBox = new QComboBox(right);
	m_modeBox->addItem(tr("Overlay"), static_cast<int>(CompareMode::Overlay));
	m_modeBox->addItem(tr("Difference per shape"), static_cast<int>(CompareMode::Difference));
	m_modeBox->setToolTip(tr("Overlay: both runs in one histogram\nDifference: compared minus selected value, for shapes in both runs"));
	compareRow->addWidget(new QLabel(tr("Compare with:"), right));
	compareRow->addWidget(m_compareBox, 1);
	compareRow->addWidget(m_modeBox);

	auto* sliderRow = new QHBoxLayout;
	sliderRow->addWidget(new QLabel(tr("Buckets:"), right));
	m_bucketSlider = new QSlider(Qt::Horizontal, right);
	m_bucketSlider->setRange(1, 200);
	m_bucketSlider->setValue(20);
	m_bucketLabel = new QLabel(right);
	m_bucketLabel->setMinimumWidth(40);
	auto* scaleBox = new QComboBox(right);
	scaleBox->addItem(tr("Linear"), static_cast<int>(HistogramView::Scale::Linear));
	scaleBox->addItem(tr("Log"), static_cast<int>(HistogramView::Scale::Log));
	scaleBox->setToolTip(tr("Log spreads out values that differ by orders of magnitude, zero and negative values are kept"));
	auto* clipCheck = new QCheckBox(tr("Clip outliers"), right);
	clipCheck->setToolTip(tr("Only spread the buckets over the 1st to 99th percentile, the values outside get a hatched bar at either side"));
	sliderRow->addWidget(m_bucketSlider, 1);
	sliderRow->addWidget(m_bucketLabel);
	sliderRow->addWidget(scaleBox);
	sliderRow->addWidget(clipCheck);
	m_view = new HistogramView(right);
	m_summaryLabel = new QLabel(right);
	m_summaryLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
	rightLayout->addLayout(compareRow);
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

	refreshRunLists();

	auto updateBucketCount = [this] { m_bucketLabel->setText(tr("%1").arg(m_view->bucketCount())); };
	connect(
		m_bucketSlider, &QSlider::valueChanged, this,
		[this, updateBucketCount](int v)
		{
			m_view->setBucketCount(v);
			updateBucketCount();
		});
	connect(
		scaleBox, &QComboBox::currentIndexChanged, this,
		[this, scaleBox](int) { m_view->setScale(static_cast<HistogramView::Scale>(scaleBox->currentData().toInt())); });
	connect(clipCheck, &QCheckBox::toggled, m_view, &HistogramView::setClipOutliers);
	connect(
		addStepButton, &QPushButton::clicked, this,
		[this]
		{
			const auto current = m_pipelineEdit->text().trimmed();
			const auto step = m_operationBox->currentData().toString();
			m_pipelineEdit->setText(
				current.isEmpty() || current == toQString(mesh::Pipeline::emptyLabel) ? step : current + toQString(mesh::Pipeline::separator) + step);
		});
	connect(clearStepsButton, &QPushButton::clicked, m_pipelineEdit, &QLineEdit::clear);
	connect(m_limitCheck, &QCheckBox::toggled, m_limitSpin, &QWidget::setEnabled);
	connect(m_computeButton, &QPushButton::clicked, this, &Statistics::compute);
	connect(m_loadButton, &QPushButton::clicked, this, &Statistics::loadCsv);
	connect(m_exportButton, &QPushButton::clicked, this, &Statistics::exportCsv);
	connect(m_descriptorExportButton, &QPushButton::clicked, this, &Statistics::exportDescriptorHistograms);
	connect(m_removeRunButton, &QPushButton::clicked, this, &Statistics::removeRun);
	connect(
		m_runList, &QListWidget::currentRowChanged, this,
		[this](int)
		{
			m_exportButton->setEnabled(m_worker == nullptr && selectedRun() != nullptr);
			m_descriptorExportButton->setEnabled(m_worker == nullptr && selectedRun() != nullptr);
			m_removeRunButton->setEnabled(selectedRun() != nullptr);
			updateColumnList();
		});
	connect(m_compareBox, &QComboBox::currentIndexChanged, this, [this](int) { updateView(); });
	connect(m_modeBox, &QComboBox::currentIndexChanged, this, [this](int) { updateView(); });
	connect(m_classFilter, &QComboBox::currentIndexChanged, this, [this](int) { updateView(); });
	connect(
		m_list, &QListWidget::currentRowChanged, this,
		[this, updateBucketCount](int)
		{
			updateView();
			updateBucketCount();
		});
	connect(m_view, &HistogramView::bucketClicked, this, &Statistics::onBucketClicked);
	updateBucketCount();
}

void Statistics::setBusy(const bool busy)
{
	m_computeButton->setEnabled(!busy);
	m_loadButton->setEnabled(!busy);
	m_exportButton->setEnabled(!busy && selectedRun() != nullptr);
	m_descriptorExportButton->setEnabled(!busy && selectedRun() != nullptr);
	m_descriptorBins->setEnabled(!busy);
	m_progress->setVisible(busy);
}

void Statistics::compute()
{
	if (m_worker != nullptr)
	{
		return;
	}

	auto pipeline = mesh::Pipeline::parse(m_pipelineEdit->text().toStdString());
	if (!pipeline)
	{
		m_statusLabel->setText(tr("Unknown operation in the pipeline, use the operations from the list"));
		return;
	}

	std::vector<ShapeRef> shapes;
	if (const auto* run = selectedRun(); m_sameShapesCheck->isChecked() && run != nullptr)
	{
		shapes = run->shapes();
	}
	else
	{
		const size_t limit = m_limitCheck->isChecked() ? static_cast<size_t>(m_limitSpin->value()) : 0;
		shapes = m_database->randomClassBalancedShapes(limit);
	}

	setBusy(true);
	m_progress->setRange(0, static_cast<int>(shapes.size()));
	m_progress->setValue(0);
	m_statusLabel->setText(tr("Computing %1 for %2 shapes...").arg(QString::fromStdString(pipeline->label())).arg(shapes.size()));
	m_cancel->store(false);

	m_worker = QThread::create(
		[this, database = m_database, cancel = m_cancel, shapes = std::move(shapes), pipeline = std::move(*pipeline), progress = m_progress]
		{
			auto onProgress = [progress](const size_t done, size_t)
			{
				QMetaObject::invokeMethod(
					progress, [progress, done] { progress->setValue(std::max(progress->value(), static_cast<int>(done))); }, Qt::QueuedConnection);
			};
			auto result = std::make_shared<stats::RunResult>(stats::compute(*database, shapes, pipeline, stats::metrics(), cancel.get(), onProgress));

			QMetaObject::invokeMethod(
				this,
				[this, result]
				{
					for (const auto& path : result->failed)
					{
						qWarning() << "Failed to load mesh:" << QString::fromStdString(path.string());
					}
					const auto numShapes = result->table.rows().size();
					addRun(std::move(result->table));
					m_statusLabel->setText(
						result->failed.empty() ? tr("%1 shapes").arg(numShapes)
											   : tr("%1 shapes, %2 failed to load").arg(numShapes).arg(result->failed.size()));
				},
				Qt::QueuedConnection);
		});
	m_worker->setParent(this);
	connect(
		m_worker, &QThread::finished, this,
		[this]
		{
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

	auto table = stats::StatsTable::readCsv(toPath(path));
	if (!table)
	{
		m_statusLabel->setText(tr("Could not read %1").arg(QFileInfo(path).fileName()));
		return;
	}
	const auto numShapes = table->rows().size();
	addRun(std::move(*table));
	m_statusLabel->setText(tr("%1 shapes from %2").arg(numShapes).arg(QFileInfo(path).fileName()));
	setBusy(false);
}

void Statistics::exportCsv()
{
	const auto* run = selectedRun();
	if (run == nullptr)
	{
		return;
	}

	const QString path = QFileDialog::getSaveFileName(this, tr("Export statistics"), "shape_db_statistics.csv", tr("CSV files (*.csv)"));
	if (path.isEmpty())
	{
		return;
	}

	const bool written = run->writeCsv(toPath(path));
	m_statusLabel->setText(
		written ? tr("Exported %1 shapes to %2").arg(run->rows().size()).arg(QFileInfo(path).fileName())
				: tr("Could not write %1").arg(QFileInfo(path).fileName()));
}

void Statistics::exportDescriptorHistograms()
{
	if (m_worker != nullptr)
	{
		return;
	}

	const auto* run = selectedRun();
	if (run == nullptr)
	{
		return;
	}
	auto pipeline = pipelineFromRunLabel(run->label());
	if (!pipeline)
	{
		m_statusLabel->setText(tr("Cannot export descriptor histograms: the selected run has no recognized pipeline label"));
		return;
	}

	auto shapes = run->shapes();
	const auto classFilter = m_classFilter->currentData().toString().toStdString();
	if (!classFilter.empty())
	{
		std::erase_if(shapes, [&](const ShapeRef& shape) { return shape.className != classFilter; });
	}

	const QString csvPath = QFileDialog::getSaveFileName(
		this, tr("Export per-shape descriptor histograms"), "shape_descriptor_histograms.csv", tr("CSV files (*.csv)"));
	if (csvPath.isEmpty())
	{
		return;
	}

	const auto outputPath = toPath(csvPath);
	const auto bins = static_cast<size_t>(m_descriptorBins->value());
	setBusy(true);
	m_progress->setRange(0, static_cast<int>(shapes.size()));
	m_progress->setValue(0);
	m_statusLabel->setText(tr("Computing descriptor histograms for %1 shapes...").arg(shapes.size()));
	m_cancel->store(false);

	m_worker = QThread::create(
		[this, database = m_database, cancel = m_cancel, shapes = std::move(shapes), pipeline = std::move(*pipeline), outputPath, bins,
		 progress = m_progress, fileName = QFileInfo(csvPath).fileName()]
		{
			auto onProgress = [progress](const size_t done, size_t)
			{
				QMetaObject::invokeMethod(
					progress, [progress, done] { progress->setValue(std::max(progress->value(), static_cast<int>(done))); }, Qt::QueuedConnection);
			};
			auto result = std::make_shared<stats::HistogramCsvResult>(
				stats::writePerShapeHistogramsCsv(*database, shapes, pipeline, outputPath, bins, cancel.get(), onProgress));

			QMetaObject::invokeMethod(
				this,
				[this, result, fileName]
				{
					for (const auto& path : result->failed)
					{
						qWarning() << "Failed to compute descriptor histograms for:" << QString::fromStdString(path.string());
					}
					m_statusLabel->setText(
						result->written
							? tr("Exported %1 descriptor histograms for %2 shapes to %3 (%4 shapes failed)")
								  .arg(result->histogramCount)
								  .arg(result->shapesWritten)
								  .arg(fileName)
								  .arg(result->failed.size())
							: tr("Could not write %1").arg(fileName));
				},
				Qt::QueuedConnection);
		});
	m_worker->setParent(this);
	connect(
		m_worker, &QThread::finished, this,
		[this]
		{
			m_worker->deleteLater();
			m_worker = nullptr;
			setBusy(false);
		});
	m_worker->start();
}

void Statistics::addRun(stats::StatsTable table)
{
	const auto baseLabel = table.label();
	for (int number = 2; std::ranges::any_of(m_runs, [&](const auto& run) { return run.label() == table.label(); }); ++number)
	{
		table.setLabel(std::format("{} ({})", baseLabel, number));
	}

	m_runs.push_back(std::move(table));
	refreshRunLists();
	updateClassFilter();
	m_runList->setCurrentRow(static_cast<int>(m_runs.size()) - 1);
}

void Statistics::removeRun()
{
	const int row = m_runList->currentRow();
	if (row < 0 || row >= static_cast<int>(m_runs.size()))
	{
		return;
	}

	m_runs.erase(m_runs.begin() + row);
	refreshRunLists();
	updateClassFilter();
	m_runList->setCurrentRow(std::min(row, static_cast<int>(m_runs.size()) - 1));
	updateColumnList();
}

void Statistics::refreshRunLists()
{
	const int selected = m_runList->currentRow();
	const QString compared = m_compareBox->currentData().toString();

	m_runList->blockSignals(true);
	m_compareBox->blockSignals(true);

	m_runList->clear();
	m_compareBox->clear();
	m_compareBox->addItem(tr("Nothing"), QString());
	for (const auto& run : m_runs)
	{
		const auto label = QString::fromStdString(run.label());
		m_runList->addItem(tr("%1  (%2 shapes)").arg(label).arg(run.rows().size()));
		m_compareBox->addItem(label, label);
	}
	m_runList->setCurrentRow(std::min(selected, static_cast<int>(m_runs.size()) - 1));
	m_compareBox->setCurrentIndex(std::max(0, m_compareBox->findData(compared)));

	m_runList->blockSignals(false);
	m_compareBox->blockSignals(false);
}

void Statistics::updateClassFilter()
{
	const QString previous = m_classFilter->currentData().toString();

	std::set<std::string> classNames;
	for (const auto& run : m_runs)
	{
		for (const auto& row : run.rows())
		{
			classNames.insert(row.className);
		}
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

void Statistics::updateColumnList()
{
	const QString previous = (m_list->currentItem() != nullptr) ? m_list->currentItem()->text() : QString();

	m_list->blockSignals(true);
	m_list->clear();
	if (const auto* run = selectedRun())
	{
		for (const auto& column : run->columns())
		{
			auto* item = new QListWidgetItem(QString::fromStdString(column), m_list);
			if (const auto metric = stats::findMetric(column))
			{
				item->setToolTip(toQString(metric->description));
			}
		}
	}
	m_list->blockSignals(false);

	const auto found = m_list->findItems(previous, Qt::MatchExactly);
	// Keep showing the same column when switching runs, otherwise show the first one (row -1 if there are none)
	const int firstRow = m_list->count() == 0 ? -1 : 0;
	m_list->setCurrentRow(found.isEmpty() ? firstRow : m_list->row(found.front()));
	updateView();
}

const stats::StatsTable* Statistics::selectedRun() const
{
	const int row = m_runList->currentRow();
	return row >= 0 && row < static_cast<int>(m_runs.size()) ? &m_runs[static_cast<size_t>(row)] : nullptr;
}

const stats::StatsTable* Statistics::comparisonRun() const
{
	const auto label = m_compareBox->currentData().toString().toStdString();
	const auto found = std::ranges::find(m_runs, label, &stats::StatsTable::label);
	return label.empty() || found == m_runs.end() ? nullptr : &*found;
}

QVector<Sample> Statistics::samples(const stats::StatsTable& run, const size_t column, const std::string& classFilter) const
{
	QVector<Sample> result;
	result.reserve(static_cast<qsizetype>(run.rows().size()));
	for (const auto& row : run.rows())
	{
		if (classFilter.empty() || row.className == classFilter)
		{
			result.push_back(Sample(row.values[column], m_database->resourcePath(row.className, row.fileName)));
		}
	}
	return result;
}

void Statistics::updateView()
{
	m_view->setHistogram(nullptr);
	m_primary.reset();
	m_comparison.reset();
	m_summaryLabel->clear();

	const auto* run = selectedRun();
	if (run == nullptr || m_list->currentItem() == nullptr)
	{
		return;
	}

	const auto columnName = m_list->currentItem()->text().toStdString();
	const auto column = run->columnIndex(columnName);
	if (!column)
	{
		return;
	}

	const std::string classFilter = m_classFilter->currentData().toString().toStdString();
	const auto* other = comparisonRun();
	const auto otherColumn = other != nullptr ? other->columnIndex(columnName) : std::nullopt;
	if (other != nullptr && !otherColumn)
	{
		m_summaryLabel->setText(tr("%1 has no column %2").arg(QString::fromStdString(other->label())).arg(QString::fromStdString(columnName)));
	}

	const auto mode = static_cast<CompareMode>(m_modeBox->currentData().toInt());
	if (otherColumn && mode == CompareMode::Difference)
	{
		// Only shapes that are in both runs can be compared one by one
		QVector<Sample> differences;
		for (const auto& [first, second] : stats::matchRows(*run, *other))
		{
			const auto& row = run->rows()[first];
			if (classFilter.empty() || row.className == classFilter)
			{
				const double difference = other->rows()[second].values[*otherColumn] - row.values[*column];
				differences.push_back(Sample(difference, m_database->resourcePath(row.className, row.fileName)));
			}
		}

		const auto name = tr("%1 minus %2").arg(QString::fromStdString(other->label())).arg(QString::fromStdString(run->label()));
		m_primary.emplace(name, Histogram::defaultBucketCount(static_cast<size_t>(differences.size())), differences);
		m_view->setHistogram(&*m_primary);
		m_summaryLabel->setText(tr("%1\n%2").arg(name).arg(summaryText(*m_primary)));
	}
	else
	{
		auto primarySamples = samples(*run, *column, classFilter);
		m_primary.emplace(
			QString::fromStdString(run->label()), Histogram::defaultBucketCount(static_cast<size_t>(primarySamples.size())), primarySamples);
		QString summary = otherColumn ? tr("%1: %2").arg(m_primary->name).arg(summaryText(*m_primary)) : summaryText(*m_primary);

		if (otherColumn)
		{
			auto otherSamples = samples(*other, *otherColumn, classFilter);
			m_comparison.emplace(QString::fromStdString(other->label()), m_primary->bucket_size, otherSamples);
			summary += tr("\n%1: %2").arg(m_comparison->name).arg(summaryText(*m_comparison));
		}
		m_view->setHistograms(&*m_primary, m_comparison ? &*m_comparison : nullptr);
		m_summaryLabel->setText(summary);
	}

	m_bucketSlider->setValue(static_cast<int>(m_primary->bucket_size));
	m_view->setBucketCount(m_bucketSlider->value());
}

void Statistics::onBucketClicked(const QVector<Sample>& samples)
{
	if (samples.isEmpty())
	{
		return;
	}
	const Sample& pick = samples[QRandomGenerator::global()->bounded(samples.size())];
	emit openModel(pick.source);
}
