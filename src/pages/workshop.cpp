//
// Created by dafitius on 18-09-2026.
//

#include "workshop.h"

#include "../components/meshviewport.h"
#include "../io/off_file.hpp"
#include "../render/rendermesh.h"
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

void Workshop::loadMesh(const Mesh& mesh)
{
	auto renderMesh = RenderMesh::fromPolygons(mesh.positions(), mesh.faces());

	auto stats = QString("Vertices: %1 | Faces: %2").arg(mesh.num_vertices()).arg(mesh.num_faces());
	if (renderMesh.skippedFaces > 0)
	{
		stats += QString("\nSkipped invalid faces: %1").arg(renderMesh.skippedFaces);
	}
	statsLabel->setText(stats);

	viewport->setMesh(std::move(renderMesh));
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

	auto* resetViewBtn = new QPushButton("Reset view", sidebar);
	sidebarLayout->addWidget(resetViewBtn);

	auto* controlsLabel =
		new QLabel("Left drag: rotate\nRight/middle drag or Shift + left drag: pan\nScroll: zoom\nDouble click: reset view", sidebar);
	controlsLabel->setObjectName("pageSubtitle");
	controlsLabel->setWordWrap(true);
	sidebarLayout->addWidget(controlsLabel);

	sidebarLayout->addStretch();

	connect(
		shadedBox, &QCheckBox::toggled, this,
		[this](const bool checked)
		{
			auto settings = viewport->renderSettings();
			settings.showSurface = checked;
			viewport->setRenderSettings(settings);
		});

	connect(
		edgesBox, &QCheckBox::toggled, this,
		[this](const bool checked)
		{
			auto settings = viewport->renderSettings();
			settings.showEdges = checked;
			viewport->setRenderSettings(settings);
		});

	connect(resetViewBtn, &QPushButton::clicked, viewport, &MeshViewport::resetView);

	splitter->setStretchFactor(0, 2);
	splitter->setStretchFactor(1, 1);
	splitter->setSizes({800, 300});
}
