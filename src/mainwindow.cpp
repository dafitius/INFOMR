#include "mainwindow.h"
#include "geometry/mesh.hpp"
#include "pages/browsepage.h"
#include "pages/workshop.h"
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(std::unique_ptr<ShapeDb> database, QWidget* parent)
	: QMainWindow(parent)
{
	setupUI(std::move(database));
}

MainWindow::~MainWindow() {}

void MainWindow::setupUI(std::unique_ptr<ShapeDb> database)
{
	auto* centralWidget = new QWidget(this);
	auto* layout = new QVBoxLayout(centralWidget);
	layout->setContentsMargins(0, 0, 0, 0);

	tabWidget = new QTabWidget(this);

	// Browse tab
	auto* browsePage = new BrowsePage(std::move(database), this);
	tabWidget->addTab(browsePage, "Browse");

	// Workshop tab
	auto* workshop = new Workshop(this);
	tabWidget->addTab(workshop, "Workshop");

	connect(
		browsePage, &BrowsePage::meshSelected, this,
		[this, workshop](const std::filesystem::path& source)
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

			workshop->loadMesh(*mesh);
			tabWidget->setCurrentWidget(workshop);
		});

	layout->addWidget(tabWidget);
	setCentralWidget(centralWidget);
}