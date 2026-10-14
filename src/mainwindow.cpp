#include "mainwindow.h"

#include "components/histogram.h"
#include "geometry/mesh.hpp"
#include "pages/browsepage.h"
#include "pages/statistics.h"
#include "pages/workshop.h"
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(std::shared_ptr<ShapeDb> database, QWidget* parent)
	: QMainWindow(parent), database_(database)
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

	auto* stats = new Statistics(this);
	stats->setProvider(
		[&](const int limit)
		{
			QVector<Histogram> result;

			//Get all paths we want to analyse, this is where we limit the population
			std::vector<std::filesystem::path> work;
			for (const auto& meshPath : database_->iterRandomShapesClassDistributed(limit))
			{
				work.emplace_back(meshPath);
			}

			//Create [mesh, path] pairs using a parallel foreach
			std::vector<std::pair<std::optional<Mesh>, std::filesystem::path>> meshes(work.size());
			auto indices = std::views::iota(size_t{0}, work.size());

			std::for_each(
				std::execution::par, indices.begin(), indices.end(),
				[&](const size_t i)
				{
					if (auto mesh = Mesh::fromObj(work[i]))
					{
						meshes[i].first.emplace(std::move(*mesh));
						meshes[i].second = std::move(work[i]);
					}
				});

			//lambda taking a name, size and callback in order to create the histogram.
			//Could maybe be moved to be a constructor on Historgram, but you'd need to pass in meshes and limit
			auto createHistogram = [&meshes, limit] (const std::string& name, const size_t bucketSize, const std::function<double(const Mesh&)>& callback)
			{
				auto samples = QVector<Sample>{};
				samples.reserve(limit);
				for (const auto& [mesh, path] : meshes)
				{
					if (mesh)
					{
						samples.push_back(Sample(callback(mesh.value()), path));
					}
				}
				return Histogram{name.c_str(), bucketSize, samples};
			};

			//example usage of createHistogram
			result.push_back(createHistogram("Number of vertices", 2000, [](const auto& mesh) -> double
			{
				return mesh.numVertices();
			}));

			//After creating this histogram you can freely modify the population if that's required for other stats
			return result;
		});
	tabWidget->addTab(stats, tr("Statistics"));

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