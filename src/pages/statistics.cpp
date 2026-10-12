#include "statistics.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMouseEvent>
#include <QPushButton>
#include <QRandomGenerator>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QToolTip>
#include <QVBoxLayout>
#include <algorithm>

Statistics::Statistics(QWidget* parent) : QWidget(parent)
{
	setupUI();
}

Statistics::~Statistics()
{
}

void Statistics::setProvider(Provider provider)
{
	m_provider = std::move(provider);
}

void Statistics::setupUI()
{
	auto* root = new QVBoxLayout(this);

	auto* top = new QHBoxLayout;
	top->setContentsMargins(6, 6, 6, 0);
	m_refreshButton = new QPushButton(tr("Load"), this);
	// m_refreshButton->setMaximumSize(100, 50);
	m_limitCheck = new QCheckBox(tr("Limit to"), this);
	m_limitSpin = new QSpinBox(this);
	m_limitSpin->setRange(1, 5000);
	m_limitSpin->setValue(1000);
	m_limitSpin->setSuffix(tr(" models"));
	m_limitSpin->setEnabled(false);
	top->addWidget(m_refreshButton);
	top->addWidget(m_limitCheck);
	top->addWidget(m_limitSpin);
	top->addStretch();
	root->addLayout(top);


	m_splitter = new QSplitter(Qt::Horizontal, this);
	m_list = new QListWidget(m_splitter);
	m_list->setMinimumWidth(120);

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
	rightLayout->addLayout(sliderRow);
	rightLayout->addWidget(m_view, 1);

	m_splitter->addWidget(m_list);
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
	connect(m_refreshButton, &QPushButton::clicked, this, &Statistics::refresh);
	connect(m_list, &QListWidget::currentRowChanged, this, [this, updateBucketCount](int) {
		onSelectionChanged();
		updateBucketCount();
	});
	connect(m_view, &HistogramView::bucketClicked, this, &Statistics::onBucketClicked);
	updateBucketCount();
}

void Statistics::refresh()
{
	if (!m_provider) return;

	const int limit = m_limitCheck->isChecked() ? m_limitSpin->value() : 0;
	m_refreshButton->setEnabled(false);
	m_histograms = m_provider(limit);

	const QString previous = (m_list->currentItem() != nullptr) ? m_list->currentItem()->text() : QString();
	m_view->setHistogram(nullptr);
	m_list->blockSignals(true);
	m_list->clear();
	for (const auto& hist : m_histograms)
	{
		m_list->addItem(hist.name);
	}
	m_list->blockSignals(false);

	m_refreshButton->setEnabled(true);
	if (!m_histograms.isEmpty()) { m_list->setCurrentRow(0);
	} else { onSelectionChanged();
}
}

void Statistics::onSelectionChanged()
{
	int const row = m_list->currentRow();
	m_view->setHistogram(row >= 0 && row < m_histograms.size() ? &m_histograms[row] : nullptr); //Set the correct histogram
	m_bucketSlider->setValue(m_histograms[row].bucket_size); //Also adjust the bucket size to what the histogram provides as default
}

void Statistics::onBucketClicked(const QVector<Sample>& samples)
{
	if (samples.isEmpty()) return;
	const Sample& pick = samples[QRandomGenerator::global()->bounded(samples.size())];
	emit openModel(pick.source);
}