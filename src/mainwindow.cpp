#include "mainwindow.h"
#include "io/off_file.hpp"
#include "pages/browsepage.h"
#include "pages/workshop.h"
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget* parent)
	: QMainWindow(parent)
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
	auto* browsePage = new BrowsePage(this);
	tabWidget->addTab(browsePage, "Browse");

	// Workshop tab
	auto* workshop = new Workshop(this);
	tabWidget->addTab(workshop, "Workshop");

	connect(
		browsePage, &BrowsePage::meshSelected, this,
		[this, workshop](const QString& source)
		{
			if (auto obj = OffFile::read(source.toStdString()))
			{
				workshop->loadMesh(obj.value());
				tabWidget->setCurrentWidget(workshop);
			}
		});

	layout->addWidget(tabWidget);
	setCentralWidget(centralWidget);
}