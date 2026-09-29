#include "browsepage.h"

#include "../io/shape_db.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QScrollArea>
#include <QSplitter>
#include <QVBoxLayout>
#include <qquickwindow.h>

BrowsePage::BrowsePage(std::unique_ptr<ShapeDb> database, QWidget* parent)
	: QWidget(parent), database(std::move(database))
{
	setupUI();
	loadSampleMeshes();
}

BrowsePage::~BrowsePage() {}

void BrowsePage::setupUI()
{
	QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

	auto* mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(16, 16, 16, 16);

	auto* splitter = new QSplitter(Qt::Horizontal, this);
	splitter->setChildrenCollapsible(false);
	mainLayout->addWidget(splitter);

	auto* sidebar = new QWidget(splitter);
	sidebar->setObjectName("browseSidebar");
	sidebar->setMinimumWidth(220);

	auto* sidebarLayout = new QVBoxLayout(sidebar);
	sidebarLayout->setContentsMargins(8, 8, 8, 8);
	sidebarLayout->setSpacing(12);

	auto* sidebarTitle = new QLabel("Search & Filters", sidebar);
	sidebarTitle->setObjectName("pageTitle");
	sidebarLayout->addWidget(sidebarTitle);

	auto* searchInput = new QLineEdit(sidebar);
	searchInput->setPlaceholderText("Search meshes...");
	sidebarLayout->addWidget(searchInput);

	sidebarLayout->addStretch();

	auto* contentWidget = new QWidget(splitter);
	auto* contentLayout = new QVBoxLayout(contentWidget);
	contentLayout->setContentsMargins(0, 0, 0, 0);
	contentLayout->setSpacing(12);

	auto* title = new QLabel("Browse meshes", contentWidget);
	title->setObjectName("pageTitle");
	contentLayout->addWidget(title);

	scrollArea = new QScrollArea(contentWidget);
	scrollArea->setWidgetResizable(true);
	scrollArea->setFrameShape(QFrame::NoFrame);

	containerWidget = new QWidget(scrollArea);
	containerLayout = new QGridLayout(containerWidget);
	containerLayout->setContentsMargins(0, 0, 0, 0);
	containerLayout->setHorizontalSpacing(14);
	containerLayout->setVerticalSpacing(14);
	containerLayout->setColumnStretch(0, 1);
	containerLayout->setColumnStretch(1, 1);
	containerLayout->setAlignment(Qt::AlignTop);

	scrollArea->setWidget(containerWidget);
	contentLayout->addWidget(scrollArea, 1);

	splitter->setStretchFactor(0, 1);
	splitter->setStretchFactor(1, 2);
	splitter->setSizes({300, 700});
}

void BrowsePage::loadSampleMeshes()
{
	auto first_class = database->iter_class_names().front();
	;
	for (const auto& mesh : database->iter_class_shape_paths(first_class))
	{
		addMeshCard({
		QString{first_class.data()},mesh.c_str()});
	}
}

void BrowsePage::addMeshCard(const MeshData& mesh)
{
	auto* card = new BrowserCard(mesh.name, mesh.source, containerWidget);

	const int index = static_cast<int>(meshCards.size());
	const int row = index / 2;
	const int column = index % 2;

	containerLayout->addWidget(card, row, column);
	meshCards.push_back(card);

	connect(card, &BrowserCard::meshClicked, this, &BrowsePage::onMeshCardClicked);
	connect(card, &BrowserCard::meshClicked, this, [this, mesh](const QString&) { emit meshSelected(mesh.source); });
}

void BrowsePage::onMeshCardClicked(const QString& meshName)
{
	qDebug() << "Clicked mesh:" << meshName;
}
