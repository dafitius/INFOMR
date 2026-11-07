
#include "io/shape_db.hpp"
#include "mainwindow.h"
#include "querying/query.h"
#include "theme.h"
#include <array>
#include <iomanip>
#include <map>
#include <qquickwindow.h>
#include <sstream>
#include <stdexcept>
#include <string>
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
		const auto printUsage = []
		{
			std::cerr << "Usage: --evaluate <amount of queries (0=all)> <k (0=amount of shapes in class)> <weights as string, e.g. \"0.5,0.3,0.6\"> "
						 "<mode (Mix->0,L11->1,L2->2)>"
					  << std::endl;
		};
		if (argc != 6)
		{
			printUsage();
			return 2;
		}

		const auto parseInteger = [](const std::string_view text)
		{
			std::size_t parsedLength = 0;
			const auto value = std::stoll(std::string{text}, &parsedLength);
			if (parsedLength != text.size())
				throw std::invalid_argument("expected an integer");
			return value;
		};

		long long queryCountArgument;
		long long kArgument;
		long long modeArgument;
		try
		{
			queryCountArgument = parseInteger(argv[2]);
			kArgument = parseInteger(argv[3]);
			modeArgument = parseInteger(argv[5]);
		}
		catch (const std::invalid_argument& error)
		{
			std::cerr << "Invalid evaluation arguments: " << error.what() << std::endl;
			printUsage();
			return 2;
		}
		catch (const std::out_of_range& error)
		{
			std::cerr << "Evaluation argument is out of range: " << error.what() << std::endl;
			printUsage();
			return 2;
		}
		if (queryCountArgument < 0 || kArgument < 0 || modeArgument < 0 || modeArgument > 2)
		{
			std::cerr << "Query count and k must be non-negative, and mode must be 0, 1, or 2." << std::endl;
			printUsage();
			return 2;
		}

		std::vector<double> weights;
		std::stringstream weightStream(argv[4]);
		std::string weight;
		while (std::getline(weightStream, weight, ','))
		{
			std::size_t parsedLength = 0;
			try
			{
				const auto value = std::stod(weight, &parsedLength);
				if (parsedLength != weight.size())
					throw std::invalid_argument("expected a comma-separated list of numbers");
				weights.push_back(value);
			}
			catch (const std::invalid_argument& error)
			{
				std::cerr << "Invalid weights: " << error.what() << std::endl;
				return 2;
			}
			catch (const std::out_of_range& error)
			{
				std::cerr << "Weight is out of range: " << error.what() << std::endl;
				return 2;
			}
		}

		const auto distMode = static_cast<DistanceMode>(modeArgument);
		const auto meshes = shapeDb->randomClassBalancedShapes(static_cast<std::size_t>(queryCountArgument));
		std::cout << "Evaluation mode with " << meshes.size() << " queries and k:" << kArgument << "." << std::endl;

		struct Counts
		{
			std::size_t tp = 0;
			std::size_t fp = 0;
			std::size_t fn = 0;
			std::size_t queries = 0;
		};

		const auto metricsFor = [](const Counts& counts)
		{
			const auto precisionDenominator = counts.tp + counts.fp;
			const auto recallDenominator = counts.tp + counts.fn;
			const auto precision = precisionDenominator == 0 ? 0.0 : static_cast<double>(counts.tp) / static_cast<double>(precisionDenominator);
			const auto recall = recallDenominator == 0 ? 0.0 : static_cast<double>(counts.tp) / static_cast<double>(recallDenominator);
			const auto f1Denominator = precision + recall;
			const auto f1 = f1Denominator == 0.0 ? 0.0 : 2.0 * precision * recall / f1Denominator;
			return std::array{precision, recall, f1};
		};

		Counts total;
		std::map<std::string, Counts> perClass;
		for (std::size_t i = 0; i < meshes.size(); i++)
		{
			const auto mesh = meshes[i];
			const auto path = std::filesystem::path(std::string{SHAPE_DB_ROOT} + "/" + mesh.className + "/" + mesh.fileName);
			Query query(path, weights, distMode);
			const auto classSize = shapeDb->numShapes(mesh.className);

			const auto classK = kArgument == 0 ? classSize : static_cast<std::size_t>(kArgument);

			const auto results = query.search(classK);
			Counts queryCounts;

			for (size_t j = 0; j < results.size(); j++)
			{
				if (results[j].className == mesh.className)
					queryCounts.tp++;
				else
					queryCounts.fp++;
			}
			queryCounts.fn = classSize - 1 - queryCounts.tp;

			const auto [precision, recall, f1] = metricsFor(queryCounts);
			std::cout << std::fixed << std::setprecision(4) << "Query " << i << " Class: " << mesh.className << " - TP: " << queryCounts.tp
					  << " FP: " << queryCounts.fp << " FN: " << queryCounts.fn << " Precision: " << precision << " Recall: " << recall
					  << " F1: " << f1 << std::endl;

			auto& classCounts = perClass[mesh.className];
			classCounts.tp += queryCounts.tp;
			classCounts.fp += queryCounts.fp;
			classCounts.fn += queryCounts.fn;
			classCounts.queries++;
			total.tp += queryCounts.tp;
			total.fp += queryCounts.fp;
			total.fn += queryCounts.fn;
		}

		double macroPrecision = 0.0;
		double macroRecall = 0.0;
		double macroF1 = 0.0;
		std::size_t macroClassCount = 0;
		for (const auto& [className, counts] : perClass)
		{
			const auto [precision, recall, f1] = metricsFor(counts);
			std::cout << "CLASS " << className << " - Queries: " << counts.queries << " TP: " << counts.tp << " FP: " << counts.fp
					  << " FN: " << counts.fn << " Precision: " << precision << " Recall: " << recall << " F1: " << f1 << std::endl;
			macroPrecision += precision;
			macroRecall += recall;
			macroF1 += f1;
			macroClassCount++;
		}
		if (macroClassCount > 0)
		{
			std::cout << "MACRO AVG - Precision: " << macroPrecision / static_cast<double>(macroClassCount)
					  << " Recall: " << macroRecall / static_cast<double>(macroClassCount)
					  << " F1: " << macroF1 / static_cast<double>(macroClassCount) << std::endl;
		}
		const auto [precision, recall, f1] = metricsFor(total);
		std::cout << "TOTAL (micro) - TP: " << total.tp << " FP: " << total.fp << " FN: " << total.fn << " Precision: " << precision
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
