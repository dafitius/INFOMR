//
// Created by dafitius on 18-09-2026.
//

#include "workshop.h"
#include "../components/meshviewport.h"
#include "../geometry/features/features.h"
#include "../stats/distributions.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QVBoxLayout>

Workshop::Workshop(QWidget* parent)
	: QWidget(parent)
{
	setupUI();
}

void Workshop::loadMesh(const Mesh& mesh)
{
	const auto object = mesh;
	viewport->setMesh(object);
	fixNormalsButton->setEnabled(true);
	fillHolesButton->setEnabled(true);
	normalizeTranslationButton->setEnabled(true);
	alignAxesButton->setEnabled(true);
	normalizePoseButton->setEnabled(true);
	scaleToUnitButton->setEnabled(true);
	recalcStats();
}

void Workshop::recalcStats()
{
	updateDistribution();

	auto meshOpt = viewport->getMesh();
	if (!meshOpt.has_value())
	{
		statsLabel->setText("No mesh loaded");
		statsLabel->setWordWrap(true);
		return;
	}

	const auto& mesh = meshOpt.value();
	auto stats = QString(
					 "Vertices: %1 | Faces: %2 |\n Surface area: %3 | Volume: %4 |\n Compactness: %5 | Rectangularity: %6 |\n Convexity: %7 | "
					 "Eccentricity: %8 |\n Diameter: %9")
					 .arg(mesh.numVertices())
					 .arg(mesh.numFaces())
					 .arg(mesh::features::global_descriptors::surfaceArea(mesh))
					 .arg(mesh::features::global_descriptors::volume(mesh))
					 .arg(mesh::features::global_descriptors::compactness(mesh))
					 .arg(mesh::features::global_descriptors::rectangularity(mesh))
					 .arg(mesh::features::global_descriptors::convexity(mesh))
					 .arg(mesh::features::global_descriptors::eccentricity(mesh))
					 .arg(mesh::features::global_descriptors::diameter(mesh));

	if (mesh.numSkippedFaces() > 0)
	{
		stats += QString("\nSkipped invalid faces: %1").arg(mesh.numSkippedFaces());
	}
	statsLabel->setText(stats);
	statsLabel->setWordWrap(true);
}

void Workshop::updateDistribution()
{
	distributionView->setHistogram(nullptr);
	distributionHistogram.reset();
	distributionSummary->clear();

	const auto mesh = viewport->getMesh();
	const auto distribution = stats::findDistribution(distributionBox->currentData().toString().toStdString());
	if (!mesh || !distribution)
	{
		return;
	}

	QVector<Sample> samples;
	for (const auto value : distribution->sample(*mesh))
	{
		samples.push_back(Sample(value, {}));
	}

	const auto name = distributionBox->currentText();
	distributionHistogram.emplace(name, static_cast<size_t>(distributionBinsSpin->value()), samples);
	distributionView->setFixedRange(std::make_pair(distribution->range.min, distribution->range.max));
	distributionView->setBucketCount(distributionBinsSpin->value());
	distributionView->setHistogram(&*distributionHistogram);

	const auto summary = distributionHistogram->summary();
	distributionSummary->setText(QString("n = %1    min = %2    max = %3    mean = %4    median = %5    std = %6")
									 .arg(summary.count)
									 .arg(summary.min, 0, 'g', 4)
									 .arg(summary.max, 0, 'g', 4)
									 .arg(summary.mean, 0, 'g', 4)
									 .arg(summary.median, 0, 'g', 4)
									 .arg(summary.stddev, 0, 'g', 4));
}

QWidget* Workshop::setupDistributionPanel(QWidget* parent)
{
	auto* panel = new QWidget(parent);
	auto* layout = new QVBoxLayout(panel);
	layout->setContentsMargins(0, 8, 0, 0);

	auto* controls = new QHBoxLayout;
	distributionBox = new QComboBox(panel);
	for (const auto& distribution : stats::distributions())
	{
		const auto name = QString::fromUtf8(distribution.name.data(), static_cast<qsizetype>(distribution.name.size()));
		distributionBox->addItem(name.toUpper(), name);
		distributionBox->setItemData(
			distributionBox->count() - 1, QString::fromUtf8(distribution.description.data(), static_cast<qsizetype>(distribution.description.size())),
			Qt::ToolTipRole);
	}
	distributionBox->setToolTip("Shape property distribution of the current mesh, computed from random vertices");

	distributionBinsSpin = new QSpinBox(panel);
	distributionBinsSpin->setRange(5, 100);
	distributionBinsSpin->setValue(20);
	distributionBinsSpin->setPrefix("Bins: ");
	distributionBinsSpin->setToolTip("Number of bins over the fixed range of the descriptor, the same for every mesh");

	auto* resampleButton = new QPushButton("Resample", panel);
	resampleButton->setToolTip("Draw new random vertices, the samples differ a little every time");

	controls->addWidget(distributionBox);
	controls->addWidget(distributionBinsSpin);
	controls->addWidget(resampleButton);
	controls->addStretch();

	distributionView = new HistogramView(panel);
	distributionView->setMinimumHeight(160);
	distributionSummary = new QLabel(panel);
	distributionSummary->setTextInteractionFlags(Qt::TextSelectableByMouse);

	layout->addLayout(controls);
	layout->addWidget(distributionView, 1);
	layout->addWidget(distributionSummary);

	connect(distributionBox, &QComboBox::currentIndexChanged, this, [this](int) { updateDistribution(); });
	connect(distributionBinsSpin, &QSpinBox::valueChanged, distributionView, &HistogramView::setBucketCount);
	connect(resampleButton, &QPushButton::clicked, this, &Workshop::updateDistribution);
	return panel;
}

Workshop::~Workshop() = default;

void Workshop::setupUI()
{
	auto* mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(16, 16, 16, 16);

	auto* splitter = new QSplitter(Qt::Horizontal, this);
	splitter->setChildrenCollapsible(false);
	mainLayout->addWidget(splitter);

	auto* viewSplitter = new QSplitter(Qt::Vertical, splitter);
	viewSplitter->setChildrenCollapsible(true);

	viewport = new MeshViewport(viewSplitter);
	viewport->setObjectName("workshopViewport");
	viewSplitter->addWidget(viewport);
	viewSplitter->addWidget(setupDistributionPanel(viewSplitter));
	viewSplitter->setStretchFactor(0, 3);
	viewSplitter->setStretchFactor(1, 1);

	auto* sidebar = new QWidget(splitter);
	sidebar->setObjectName("workshopSidebar");
	sidebar->setMinimumWidth(260);

	auto* sidebarLayout = new QVBoxLayout(sidebar);
	sidebarLayout->setContentsMargins(8, 8, 8, 8);
	sidebarLayout->setSpacing(12);

	auto* title = new QLabel("Workshop", sidebar);
	title->setObjectName("pageTitle");
	sidebarLayout->addWidget(title);

	statsLabel = new QLabel("No mesh loaded", sidebar);
	statsLabel->setObjectName("meshStats");
	sidebarLayout->addWidget(statsLabel);

	auto* recalcStatsBtn = new QPushButton("Recalc stats", sidebar);
	recalcStatsBtn->setObjectName("recalcStatsButton");
	sidebarLayout->addWidget(recalcStatsBtn);

	auto* shadedBox = new QCheckBox("Shaded", sidebar);
	shadedBox->setToolTip("Render the mesh surface");
	shadedBox->setChecked(viewport->renderSettings().showSurface);
	sidebarLayout->addWidget(shadedBox);

	auto* edgesBox = new QCheckBox("Edges", sidebar);
	edgesBox->setToolTip("Render the edges of the mesh faces");
	edgesBox->setChecked(viewport->renderSettings().showEdges);
	sidebarLayout->addWidget(edgesBox);

	auto* boundingBox = new QCheckBox("Bounding box", sidebar);
	boundingBox->setToolTip("Show the bounding box of the object");
	boundingBox->setChecked(viewport->renderSettings().showBoundingBox);
	sidebarLayout->addWidget(boundingBox);

	auto* normalsBox = new QCheckBox("Show normals", sidebar);
	normalsBox->setToolTip("Show the vertex normals on the object");
	normalsBox->setChecked(viewport->renderSettings().showNormals);
	sidebarLayout->addWidget(normalsBox);

	auto* hullBox = new QCheckBox("Show Hull", sidebar);
	hullBox->setToolTip("Render a convex hull around the object");
	hullBox->setChecked(viewport->renderSettings().showHull);
	sidebarLayout->addWidget(hullBox);

	auto* gridBox = new QCheckBox("Render Grid", sidebar);
	gridBox->setToolTip("Render a grid on the ground plane");
	gridBox->setChecked(viewport->renderSettings().showGrid);
	sidebarLayout->addWidget(gridBox);

	auto* axesBox = new QCheckBox("World axes", sidebar);
	axesBox->setToolTip("Show the world coordinate axes");
	axesBox->setChecked(viewport->renderSettings().showAxes);
	sidebarLayout->addWidget(axesBox);

	auto* eigenvectorAxesBox = new QCheckBox("Eigenvector axes", sidebar);
	eigenvectorAxesBox->setToolTip("Show the mesh's principal eigenvector axes");
	eigenvectorAxesBox->setChecked(viewport->renderSettings().showEigenvectorAxes);
	sidebarLayout->addWidget(eigenvectorAxesBox);

	fixNormalsButton = new QPushButton("Repair normals", sidebar);
	fixNormalsButton->setToolTip("Makes all normals face a consistent direction, either outwards or inwards.");
	fixNormalsButton->setEnabled(false);
	sidebarLayout->addWidget(fixNormalsButton);

	fillHolesButton = new QPushButton("Fill holes", sidebar);
	fillHolesButton->setToolTip("Detect and fill holes.");
	fillHolesButton->setEnabled(false);
	sidebarLayout->addWidget(fillHolesButton);

	normalizeTranslationButton = new QPushButton("Normalize translation", sidebar);
	normalizeTranslationButton->setToolTip("Move the barycenter to the world center.");
	normalizeTranslationButton->setEnabled(false);
	sidebarLayout->addWidget(normalizeTranslationButton);

	alignAxesButton = new QPushButton("Normalize Orientation", sidebar);
	alignAxesButton->setToolTip("Rotate the mesh so its eigenvector axes align with the world axes");
	alignAxesButton->setEnabled(false);
	sidebarLayout->addWidget(alignAxesButton);

	normalizePoseButton = new QPushButton("Normalize Pose", sidebar);
	normalizePoseButton->setToolTip("Flip the mesh based on its moment");
	normalizePoseButton->setEnabled(false);
	sidebarLayout->addWidget(normalizePoseButton);

	scaleToUnitButton = new QPushButton("Scale to unit", sidebar);
	scaleToUnitButton->setToolTip("Scales the object, so the x axis is exactly 1 unit (Should only be used after aligning eigenvector)");
	scaleToUnitButton->setEnabled(false);
	sidebarLayout->addWidget(scaleToUnitButton);

	auto* resetViewBtn = new QPushButton("Reset view", sidebar);
	sidebarLayout->addWidget(resetViewBtn);

	auto* controlsLabel =
		new QLabel("Left drag: rotate\nRight/middle drag or Shift + left drag: pan\nScroll: zoom\nDouble click: reset view", sidebar);
	controlsLabel->setObjectName("pageSubtitle");
	controlsLabel->setWordWrap(true);
	sidebarLayout->addWidget(controlsLabel);

	sidebarLayout->addStretch();

	connect(gridBox, &QCheckBox::toggled, this, [this](const bool checked) { viewport->setRenderSetting(&RenderSettings::showGrid, checked); });


	connect(shadedBox, &QCheckBox::toggled, this, [this](const bool checked) { viewport->setRenderSetting(&RenderSettings::showSurface, checked); });

	connect(edgesBox, &QCheckBox::toggled, this, [this](const bool checked) { viewport->setRenderSetting(&RenderSettings::showEdges, checked); });

	connect(
		boundingBox, &QCheckBox::toggled, this,
		[this](const bool checked) { viewport->setRenderSetting(&RenderSettings::showBoundingBox, checked); });

	connect(hullBox, &QCheckBox::toggled, this, [this](const bool checked) { viewport->setRenderSetting(&RenderSettings::showHull, checked); });


	connect(normalsBox, &QCheckBox::toggled, this, [this](const bool checked) { viewport->setRenderSetting(&RenderSettings::showNormals, checked); });

	connect(axesBox, &QCheckBox::toggled, this, [this](const bool checked) { viewport->setRenderSetting(&RenderSettings::showAxes, checked); });

	connect(
		eigenvectorAxesBox, &QCheckBox::toggled, this,
		[this](const bool checked) { viewport->setRenderSetting(&RenderSettings::showEigenvectorAxes, checked); });

	connect(fixNormalsButton, &QPushButton::clicked, viewport, &MeshViewport::orientNormals);
	connect(fillHolesButton, &QPushButton::clicked, viewport, &MeshViewport::fillHoles);
	connect(normalizeTranslationButton, &QPushButton::clicked, viewport, &MeshViewport::normalizeTranslation);
	connect(alignAxesButton, &QPushButton::clicked, viewport, &MeshViewport::normalizeRotation);
	connect(normalizePoseButton, &QPushButton::clicked, viewport, &MeshViewport::normalizePose);
	connect(scaleToUnitButton, &QPushButton::clicked, viewport, &MeshViewport::scaleToUnit);
	connect(resetViewBtn, &QPushButton::clicked, viewport, &MeshViewport::resetView);
	connect(recalcStatsBtn, &QPushButton::clicked, this, &Workshop::recalcStats);

	splitter->setStretchFactor(0, 2);
	splitter->setStretchFactor(1, 1);
	splitter->setSizes({800, 300});
}
