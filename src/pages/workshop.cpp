//
// Created by dafitius on 18-09-2026.
//

#include "workshop.h"
#include "../components/meshviewport.h"
#include "../geometry/features/features.h"
#include "../stats/distributions.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
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

	runPipelineButton->setEnabled(true);
	fixNormalsButton->setEnabled(true);
	triangulateButton->setEnabled(true);
	removeNonManifoldButton->setEnabled(true);
	fillHolesButton->setEnabled(true);
	remeshButton->setEnabled(true);
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
					 "Vertices: %1 | Faces: %2 | Surface area: %3 | Volume: %4 | Compactness: %5 | Rectangularity: %6 | Convexity: %7 | "
					 "Eccentricity: %8 | Diameter: %9")
					 .arg(mesh.numVertices())
					 .arg(mesh.numFaces())
					 .arg(mesh.surfaceArea())
					 .arg(mesh.volume())
					 .arg(mesh.compactness())
					 .arg(mesh.rectangularity())
					 .arg(mesh.convexity())
					 .arg(mesh.eccentricity())
					 .arg(mesh.diameter());

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
	mainLayout->setContentsMargins(12, 12, 12, 12);
	mainLayout->setSpacing(10);

	auto* splitter = new QSplitter(Qt::Horizontal, this);
	splitter->setObjectName("workshopMainSplitter");
	splitter->setChildrenCollapsible(true);
	mainLayout->addWidget(splitter);

	auto* viewSplitter = new QSplitter(Qt::Vertical, splitter);
	viewSplitter->setObjectName("workshopViewSplitter");
	viewSplitter->setChildrenCollapsible(true);

	viewport = new MeshViewport(viewSplitter);
	viewport->setObjectName("workshopViewport");
	viewSplitter->addWidget(viewport);
	viewSplitter->addWidget(setupDistributionPanel(viewSplitter));
	viewSplitter->setStretchFactor(0, 4);
	viewSplitter->setStretchFactor(1, 1);
	viewSplitter->setSizes({650, 210});

	auto* sidebarScroll = new QScrollArea(splitter);
	sidebarScroll->setObjectName("workshopSidebarScroll");
	sidebarScroll->setWidgetResizable(true);
	sidebarScroll->setFrameShape(QFrame::NoFrame);
	sidebarScroll->setMinimumWidth(280);
	sidebarScroll->setMaximumWidth(390);

	auto* sidebar = new QWidget(sidebarScroll);
	sidebar->setObjectName("workshopSidebar");
	auto* sidebarLayout = new QVBoxLayout(sidebar);
	sidebarLayout->setContentsMargins(8, 4, 8, 8);
	sidebarLayout->setSpacing(10);

	auto* title = new QLabel("Workshop", sidebar);
	title->setObjectName("pageTitle");
	sidebarLayout->addWidget(title);

	// Statistics panel
	auto* statsGroup = new QGroupBox("Mesh statistics", sidebar);
	auto* statsLayout = new QVBoxLayout(statsGroup);
	statsLayout->setContentsMargins(10, 12, 10, 10);
	statsLayout->setSpacing(8);

	statsLabel = new QLabel("No mesh loaded", statsGroup);
	statsLabel->setObjectName("meshStats");
	statsLabel->setWordWrap(true);
	statsLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
	statsLayout->addWidget(statsLabel);

	auto* recalcStatsBtn = new QPushButton("Recalc stats", statsGroup);
	recalcStatsBtn->setObjectName("recalcStatsButton");
	statsLayout->addWidget(recalcStatsBtn);
	sidebarLayout->addWidget(statsGroup);

	// Rendering options
	auto* renderGroup = new QGroupBox("Viewport display", sidebar);
	auto* renderLayout = new QGridLayout(renderGroup);
	renderLayout->setContentsMargins(10, 12, 10, 10);
	renderLayout->setHorizontalSpacing(12);
	renderLayout->setVerticalSpacing(6);

	auto* shadedBox = new QCheckBox("Shaded surface", renderGroup);
	shadedBox->setToolTip("Render the mesh surface");
	shadedBox->setChecked(viewport->renderSettings().showSurface);
	auto* edgesBox = new QCheckBox("Edges", renderGroup);
	edgesBox->setToolTip("Render the edges of the mesh faces");
	edgesBox->setChecked(viewport->renderSettings().showEdges);
	auto* boundingBox = new QCheckBox("Bounding box", renderGroup);
	boundingBox->setToolTip("Show the bounding box of the object");
	boundingBox->setChecked(viewport->renderSettings().showBoundingBox);
	auto* normalsBox = new QCheckBox("Vertex normals", renderGroup);
	normalsBox->setToolTip("Show the vertex normals on the object");
	normalsBox->setChecked(viewport->renderSettings().showNormals);
	auto* hullBox = new QCheckBox("Convex hull", renderGroup);
	hullBox->setToolTip("Render a convex hull around the object");
	hullBox->setChecked(viewport->renderSettings().showHull);
	auto* gridBox = new QCheckBox("Ground grid", renderGroup);
	gridBox->setToolTip("Render a grid on the ground plane");
	gridBox->setChecked(viewport->renderSettings().showGrid);
	auto* axesBox = new QCheckBox("World axes", renderGroup);
	axesBox->setToolTip("Show the world coordinate axes");
	axesBox->setChecked(viewport->renderSettings().showAxes);
	auto* eigenvectorAxesBox = new QCheckBox("Principal axes", renderGroup);
	eigenvectorAxesBox->setToolTip("Show the mesh's principal eigenvector axes");
	eigenvectorAxesBox->setChecked(viewport->renderSettings().showEigenvectorAxes);

	renderLayout->addWidget(shadedBox, 0, 0);
	renderLayout->addWidget(edgesBox, 0, 1);
	renderLayout->addWidget(boundingBox, 1, 0);
	renderLayout->addWidget(normalsBox, 1, 1);
	renderLayout->addWidget(hullBox, 2, 0);
	renderLayout->addWidget(gridBox, 2, 1);
	renderLayout->addWidget(axesBox, 3, 0);
	renderLayout->addWidget(eigenvectorAxesBox, 3, 1);
	sidebarLayout->addWidget(renderGroup);

	// Pipeline operations
	auto* repairGroup = new QGroupBox("Repair mesh", sidebar);
	auto* repairLayout = new QVBoxLayout(repairGroup);
	repairLayout->setContentsMargins(10, 12, 10, 10);
	repairLayout->setSpacing(6);

	runPipelineButton = new QPushButton("Run pipeline", repairGroup);
	runPipelineButton->setToolTip("Run the full pipeline on this mesh.");
	runPipelineButton->setEnabled(false);
	repairLayout->addWidget(runPipelineButton);

	fixNormalsButton = new QPushButton("Repair normals", repairGroup);
	fixNormalsButton->setToolTip("Makes all normals face a consistent direction, either outwards or inwards.");
	fixNormalsButton->setEnabled(false);
	repairLayout->addWidget(fixNormalsButton);

	triangulateButton = new QPushButton("Triangulate", repairGroup);
	triangulateButton->setToolTip("Triangulate the mesh. (Does nothing if the mesh consists of only triangles)");
	triangulateButton->setEnabled(false);
	repairLayout->addWidget(triangulateButton);

	removeNonManifoldButton = new QPushButton("Remove non-manifold", repairGroup);
	removeNonManifoldButton->setToolTip("Removes all parts of the mesh that are non-manifold.");
	removeNonManifoldButton->setEnabled(false);
	repairLayout->addWidget(removeNonManifoldButton);

	fillHolesButton = new QPushButton("Fill holes", repairGroup);
	fillHolesButton->setToolTip("Detect and fill holes.");
	fillHolesButton->setEnabled(false);
	repairLayout->addWidget(fillHolesButton);

	remeshButton = new QPushButton("Remesh", repairGroup);
	remeshButton->setToolTip("Chooses automatically wether to simplify or subdivide the mesh to reach a consistent amount of vertices (10,000).");
	remeshButton->setEnabled(false);
	repairLayout->addWidget(remeshButton);

	normalizeTranslationButton = new QPushButton("Normalize translation", repairGroup);
	normalizeTranslationButton->setToolTip("Move the barycenter to the world center.");
	normalizeTranslationButton->setEnabled(false);
	repairLayout->addWidget(normalizeTranslationButton);

	alignAxesButton = new QPushButton("Normalize Orientation", repairGroup);
	alignAxesButton->setToolTip("Rotate the mesh so its eigenvector axes align with the world axes");
	alignAxesButton->setEnabled(false);
	repairLayout->addWidget(alignAxesButton);

	normalizePoseButton = new QPushButton("Normalize Pose", repairGroup);
	normalizePoseButton->setToolTip("Flip the mesh based on its moment");
	normalizePoseButton->setEnabled(false);
	repairLayout->addWidget(normalizePoseButton);

	scaleToUnitButton = new QPushButton("Scale to unit", repairGroup);
	scaleToUnitButton->setToolTip("Scales the object, so the x axis is exactly 1 unit (Should only be used after aligning eigenvector)");
	scaleToUnitButton->setEnabled(false);
	repairLayout->addWidget(scaleToUnitButton);

	auto* resetViewBtn = new QPushButton("Reset view", repairGroup);
	repairLayout->addWidget(resetViewBtn);

	auto* exportMeshBtn = new QPushButton("Export mesh", repairGroup);
	repairLayout->addWidget(exportMeshBtn);

	sidebarLayout->addWidget(repairGroup);

	auto* controlsLabel =
		new QLabel("Left drag: rotate\nRight/middle drag or Shift + left drag: pan\nScroll: zoom\nDouble click: reset view", sidebar);
	controlsLabel->setObjectName("pageSubtitle");
	controlsLabel->setWordWrap(true);
	sidebarLayout->addWidget(controlsLabel);
	sidebarLayout->addStretch();

	sidebarScroll->setWidget(sidebar);
	splitter->addWidget(viewSplitter);
	splitter->addWidget(sidebarScroll);
	splitter->setStretchFactor(0, 1);
	splitter->setStretchFactor(1, 4);
	splitter->setSizes({1000, 330});

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

	connect(runPipelineButton, &QPushButton::clicked, viewport, &MeshViewport::runFullPipeline);

	connect(fixNormalsButton, &QPushButton::clicked, viewport, &MeshViewport::orientNormals);
	connect(triangulateButton, &QPushButton::clicked, viewport, &MeshViewport::triangulate);
	connect(removeNonManifoldButton, &QPushButton::clicked, viewport, &MeshViewport::removeNonManifold);
	connect(fillHolesButton, &QPushButton::clicked, viewport, &MeshViewport::fillHoles);
	connect(remeshButton, &QPushButton::clicked, viewport, &MeshViewport::remesh);
	connect(normalizeTranslationButton, &QPushButton::clicked, viewport, &MeshViewport::normalizeTranslation);
	connect(alignAxesButton, &QPushButton::clicked, viewport, &MeshViewport::normalizeRotation);
	connect(normalizePoseButton, &QPushButton::clicked, viewport, &MeshViewport::normalizePose);
	connect(scaleToUnitButton, &QPushButton::clicked, viewport, &MeshViewport::scaleToUnit);

	connect(resetViewBtn, &QPushButton::clicked, viewport, &MeshViewport::resetView);
	connect(recalcStatsBtn, &QPushButton::clicked, this, &Workshop::recalcStats);
	connect(exportMeshBtn, &QPushButton::clicked, viewport, &MeshViewport::exportMesh);
}
