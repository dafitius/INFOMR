//
// Created by dafitius on 18-09-2026.
//

#include "workshop.h"
#include "../components/meshviewport.h"
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>

Workshop::Workshop(QWidget* parent)
	: QWidget(parent)
{
	setupUI();
}

void Workshop::loadMesh(const Mesh& mesh) const
{
	auto stats = QString("Vertices: %1 | Faces: %2").arg(mesh.numVertices()).arg(mesh.numFaces());
	if (mesh.numSkippedFaces() > 0)
	{
		stats += QString("\nSkipped invalid faces: %1").arg(mesh.numSkippedFaces());
	}
	statsLabel->setText(stats);

	const auto object = mesh;
	viewport->setMesh(object);
	fixNormalsButton->setEnabled(true);
	fillHolesButton->setEnabled(true);
	normalizeTranslationButton->setEnabled(true);
	alignAxesButton->setEnabled(true);
	scaleToUnitButton->setEnabled(true);
}

Workshop::~Workshop() = default;

void Workshop::setupUI()
{
	auto* mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(16, 16, 16, 16);

	auto* splitter = new QSplitter(Qt::Horizontal, this);
	splitter->setChildrenCollapsible(false);
	mainLayout->addWidget(splitter);

	viewport = new MeshViewport(splitter);
	viewport->setObjectName("workshopViewport");

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
	connect(scaleToUnitButton, &QPushButton::clicked, viewport, &MeshViewport::scaleToUnit);
	connect(resetViewBtn, &QPushButton::clicked, viewport, &MeshViewport::resetView);

	splitter->setStretchFactor(0, 2);
	splitter->setStretchFactor(1, 1);
	splitter->setSizes({800, 300});
}
