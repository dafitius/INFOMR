#include "mainwindow.h"
#include "io/mesh.hpp"
#include "io/obj_file.hpp"
#include "io/off_file.hpp"
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
		[this, workshop](const QString& source)
		{
			const std::filesystem::path path = source.toStdString();
			std::optional<Mesh> mesh;

			if (path.extension() == ".obj")
			{
				mesh.emplace(*Mesh::from_obj(path));
			}
			// else if (path.extension() == ".off")
			// {
			// 	mesh = OffFile::read(path);
			// }
			else
			{
				std::cerr << "Unsupported mesh format: " << path.extension() << '\n';
				return;
			}

			if (!mesh)
			{
				std::cerr << "Failed to load mesh: " << path << '\n';
				return;
			}

			workshop->loadMesh(*mesh);
			tabWidget->setCurrentWidget(workshop);
		});

	layout->addWidget(tabWidget);
	setCentralWidget(centralWidget);
}