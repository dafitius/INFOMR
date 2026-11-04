#include "mainwindow.h"

#include "geometry/mesh.hpp"
#include "pages/browsepage.h"
#include "pages/querypage.h"
#include "pages/statistics.h"
#include "pages/workshop.h"
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(std::shared_ptr<ShapeDb> database, QWidget* parent)
	: QMainWindow(parent)
	, database_(database)
{
	setupUI();
}

MainWindow::~MainWindow() {}

void MainWindow::setupUI()
{
	auto* centralWidget = new QWidget(this);
	auto* layout = new QVBoxLayout(centralWidget);
	layout->setContentsMargins(0, 0, 0, 0);

	tabWidget = new QTabWidget(this);

	// Browse tab
	auto* browsePage = new BrowsePage(database_, this);
	tabWidget->addTab(browsePage, "Browse");

	// Workshop tab
	auto* workshop = new Workshop(this);
	tabWidget->addTab(workshop, "Workshop");

	// Statistics tab
	auto* stats = new Statistics(database_, this);
	tabWidget->addTab(stats, tr("Statistics"));

	// Query tab
	auto* query = new QueryPage(this);
	tabWidget->addTab(query, "Query");

	connect(stats, &Statistics::openModel, this, [this](const std::filesystem::path& source) { openModel(source); });
	connect(browsePage, &BrowsePage::meshSelected, this, [this](const std::filesystem::path& source) { openModel(source); });

	layout->addWidget(tabWidget);
	setCentralWidget(centralWidget);
}

void MainWindow::openModel(std::filesystem::path source)
{
	std::optional<Mesh> mesh;

	if (source.extension() == ".obj")
	{
		mesh.emplace(*Mesh::fromObj(source));
	}
	else
	{
		std::cerr << "Unsupported mesh format: " << source.extension() << '\n';
		return;
	}

	if (!mesh)
	{
		std::cerr << "Failed to load mesh: " << source << '\n';
		return;
	}

	if (auto* const workshop = qobject_cast<Workshop*>(tabWidget->widget(1)); workshop != nullptr)
	{
		workshop->loadMesh(*mesh);
		tabWidget->setCurrentWidget(workshop);
	}
}