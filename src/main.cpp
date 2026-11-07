
#include "io/shape_db.hpp"
#include "mainwindow.h"
#include "querying/query.h"
#include "theme.h"
#include <qquickwindow.h>
#include <sstream>
#include <string_view>
#include <vector>

int main(int argc, char* argv[])
{
	constexpr auto theme = theme::Mode::Dark;
	// constexpr auto theme = theme::Mode::Light;

	// auto shapeDb = ShapeDb::open(TEST_SHAPE_DB_ROOT);
	auto shapeDb = ShapeDb::open(SHAPE_DB_ROOT);

	if (!shapeDb)
	{
		std::cerr << "Failed to create ShapeDB\n";
		return 1;
	}

	if (argc >= 2 && std::string_view{argv[1]} == "--evaluate")
	{
		std::cout << "Usage: --evaluate <amount of queries> <k> <weights as string, e.g. \"0.5,0.3,0.6\"> <mode (Mix->1,L11->2,L2->3)>" << std::endl;

		size_t queryCount = std::stoi(argv[2]);
		auto k = std::stoi(argv[3]);
		auto mode = std::stoi(argv[5]);
		std::cout << "Evaluation mode with " << queryCount << " queries and k:" << k << "." << std::endl;

		auto true_positive = 0;
		auto false_postive = 0;
		auto false_negative = 0;

		std::vector<double> weights;
		std::stringstream weightStream(argv[4]);
		std::string weight;
		while (std::getline(weightStream, weight, ','))
		{
			weights.push_back(std::stod(weight));
		}

		const auto distMode = static_cast<DistanceMode>(mode);
		const auto meshes = shapeDb->randomClassBalancedShapes(queryCount);

		for (size_t i = 0; i < queryCount; i++)
		{
			const auto mesh = meshes[i];
			const auto path = std::filesystem::path(std::string{SHAPE_DB_ROOT} + "/" + mesh.className + "/" + mesh.fileName);
			Query query(path, weights, distMode);
			auto classSize = shapeDb->numShapes(mesh.className);
			if (k == 0)
				k = classSize;

			const auto results = query.search(k);
			auto tp = 0;
			auto fp = 0;
			auto fn = 0;

			for (size_t j = 0; j < results.size(); j++)
			{
				if (results[j].className == mesh.className)
				{
					tp++;
				}
				else
				{
					fp++;
				}
			}
			fn += classSize - tp;

			auto precision = static_cast<float>(tp) / (tp + fp);
			auto recall = static_cast<float>(tp) / (tp + fn);
			auto f1 = 2 * ((precision * recall) / (precision + recall));
			std::cout << "Query " << i << " Class: " << mesh.className << " - TP: " << tp << " FP: " << fp << " FN: " << fn
					  << " Precision: " << precision << " Recall: " << recall << " F1: " << f1 << std::endl;
			true_positive += tp;
			false_postive += fp;
			false_negative += fn;
		}

		auto precision = static_cast<float>(true_positive) / (true_positive + false_postive);
		auto recall = static_cast<float>(true_positive) / (true_positive + false_negative);
		auto f1 = 2 * ((precision * recall) / (precision + recall));
		std::cout << "TOTAL - TP: " << true_positive << " FP: " << false_postive << " FN: " << false_negative << " Precision: " << precision
				  << " Recall: " << recall << " F1: " << f1 << std::endl;

		return 0;
	}

	QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

	QSurfaceFormat format;
	format.setDepthBufferSize(24);
	format.setStencilBufferSize(8);
	format.setVersion(3, 3);
	format.setProfile(QSurfaceFormat::CompatibilityProfile);
	QSurfaceFormat::setDefaultFormat(format);

	QApplication app(argc, argv);
	theme::apply(app, theme);

	MainWindow window(std::make_unique<ShapeDb>(*shapeDb));
	window.setWindowTitle("Geometry Retrieval");
	window.resize(1000, 700);
	window.show();

	return QApplication::exec();
}
