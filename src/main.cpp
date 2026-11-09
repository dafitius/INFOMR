
#include "io/shape_db.hpp"
#include "mainwindow.h"
#include "querying/query.h"
#include "theme.h"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <optional>
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
						 "<mode (Mix->0,L11->1,L2->2)> [ROC CSV output path]"
					  << std::endl;
		};
		if (argc != 6 && argc != 7)
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

		std::ofstream rocCsv;
		std::ofstream scoreCsv;
		std::filesystem::path scoreCsvPath;
		if (argc == 7)
		{
			rocCsv.open(argv[6]);
			if (!rocCsv)
			{
				std::cerr << "Failed to open ROC CSV output: " << argv[6] << std::endl;
				return 1;
			}
			rocCsv << "query_index,query_class,query_file,threshold,false_positive_rate,true_positive_rate\n";

			const std::filesystem::path rocCsvPath{argv[6]};
			scoreCsvPath = rocCsvPath.parent_path() / (rocCsvPath.stem().string() + "_scores" + rocCsvPath.extension().string());
			scoreCsv.open(scoreCsvPath);
			if (!scoreCsv)
			{
				std::cerr << "Failed to open ROC candidate scores output: " << scoreCsvPath.string() << std::endl;
				return 1;
			}
			scoreCsv << "query_index,query_class,query_file,candidate_class,candidate_file,distance,relevant\n";
		}

		struct Counts
		{
			std::size_t tp = 0;
			std::size_t fp = 0;
			std::size_t fn = 0;
			std::size_t queries = 0;
			double auroc = 0.0;
			std::size_t aurocQueries = 0;
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

		const auto writeCsvField = [](std::ostream& output, const std::string& field)
		{
			output << '"';
			for (const auto character : field)
			{
				if (character == '"')
					output << "\"\"";
				else
					output << character;
			}
			output << '"';
		};

		const auto aurocFor = [&writeCsvField](
								  const std::vector<QueryResult>& rankedResults, const std::string& relevantClass, std::ostream* curveCsv,
								  const std::size_t queryIndex, const ShapeRef& queryShape) -> std::optional<double>
		{
			const auto positiveCount = static_cast<std::size_t>(
				std::ranges::count_if(rankedResults, [&relevantClass](const auto& result) { return result.className == relevantClass; }));
			const auto negativeCount = rankedResults.size() - positiveCount;
			if (positiveCount == 0 || negativeCount == 0)
				return std::nullopt;

			std::streamsize originalPrecision = 0;
			if (curveCsv)
			{
				const auto writePoint = [&](const std::optional<double> threshold, const double falsePositiveRate, const double truePositiveRate)
				{
					*curveCsv << queryIndex << ',';
					writeCsvField(*curveCsv, queryShape.className);
					*curveCsv << ',';
					writeCsvField(*curveCsv, queryShape.fileName);
					*curveCsv << ',';
					if (threshold)
						*curveCsv << *threshold;
					*curveCsv << ',' << falsePositiveRate << ',' << truePositiveRate << '\n';
				};
				originalPrecision = curveCsv->precision();
				*curveCsv << std::setprecision(17);
				writePoint(std::nullopt, 0.0, 0.0);
			}

			std::size_t seenPositives = 0;
			std::size_t seenNegatives = 0;
			double previousFalsePositiveRate = 0.0;
			double previousTruePositiveRate = 0.0;
			double area = 0.0;
			for (std::size_t i = 0; i < rankedResults.size();)
			{
				auto end = i + 1;
				while (end < rankedResults.size() && rankedResults[end].distance == rankedResults[i].distance)
					end++;

				std::size_t tiedPositives = 0;
				std::size_t tiedNegatives = 0;
				for (auto j = i; j < end; j++)
				{
					if (rankedResults[j].className == relevantClass)
						tiedPositives++;
					else
						tiedNegatives++;
				}
				seenPositives += tiedPositives;
				seenNegatives += tiedNegatives;
				const auto falsePositiveRate = static_cast<double>(seenNegatives) / static_cast<double>(negativeCount);
				const auto truePositiveRate = static_cast<double>(seenPositives) / static_cast<double>(positiveCount);
				area += (falsePositiveRate - previousFalsePositiveRate) * (truePositiveRate + previousTruePositiveRate) / 2.0;
				if (curveCsv)
				{
					*curveCsv << queryIndex << ',';
					writeCsvField(*curveCsv, queryShape.className);
					*curveCsv << ',';
					writeCsvField(*curveCsv, queryShape.fileName);
					*curveCsv << ',' << std::setprecision(17) << rankedResults[i].distance << ',' << falsePositiveRate << ',' << truePositiveRate
							  << '\n';
				}
				previousFalsePositiveRate = falsePositiveRate;
				previousTruePositiveRate = truePositiveRate;
				i = end;
			}

			if (curveCsv)
				*curveCsv << std::setprecision(originalPrecision);
			return area;
		};

		Counts total;
		std::map<std::string, Counts> perClass;
		double totalAuroc = 0.0;
		std::size_t totalAurocQueries = 0;
		for (std::size_t i = 0; i < meshes.size(); i++)
		{
			const auto mesh = meshes[i];
			const auto path = std::filesystem::path(std::string{SHAPE_DB_ROOT} + "/" + mesh.className + "/" + mesh.fileName);
			Query query(path, weights, distMode);
			const auto classSize = shapeDb->numShapes(mesh.className);

			const auto classK = kArgument == 0 ? classSize : static_cast<std::size_t>(kArgument);

			const auto results = query.search(shapeDb->numShapes(), false);
			Counts queryCounts;

			for (size_t j = 0; j < std::min(classK, results.size()); j++)
			{
				if (results[j].className == mesh.className)
					queryCounts.tp++;
				else
					queryCounts.fp++;
			}
			queryCounts.fn = classSize - 1 - queryCounts.tp;

			if (argc == 7)
			{
				scoreCsv << std::setprecision(17);
				for (const auto& result : results)
				{
					scoreCsv << i << ',';
					writeCsvField(scoreCsv, mesh.className);
					scoreCsv << ',';
					writeCsvField(scoreCsv, mesh.fileName);
					scoreCsv << ',';
					writeCsvField(scoreCsv, result.className);
					scoreCsv << ',';
					writeCsvField(scoreCsv, result.name);
					scoreCsv << ',' << result.distance << ',' << (result.className == mesh.className ? 1 : 0) << '\n';
				}
			}

			const auto [precision, recall, f1] = metricsFor(queryCounts);
			const auto auroc = aurocFor(results, mesh.className, argc == 7 ? &rocCsv : nullptr, i, mesh);
			std::cout << std::fixed << std::setprecision(4) << "Query " << i << " Class: " << mesh.className << " - TP: " << queryCounts.tp
					  << " FP: " << queryCounts.fp << " FN: " << queryCounts.fn << " Precision: " << precision << " Recall: " << recall
					  << " F1: " << f1 << " AUROC: ";
			if (auroc)
			{
				std::cout << *auroc;
				totalAuroc += *auroc;
				totalAurocQueries++;
			}
			else
				std::cout << "N/A";
			std::cout << std::endl;

			auto& classCounts = perClass[mesh.className];
			classCounts.tp += queryCounts.tp;
			classCounts.fp += queryCounts.fp;
			classCounts.fn += queryCounts.fn;
			classCounts.queries++;
			if (auroc)
			{
				classCounts.auroc += *auroc;
				classCounts.aurocQueries++;
			}
			total.tp += queryCounts.tp;
			total.fp += queryCounts.fp;
			total.fn += queryCounts.fn;
		}

		double macroPrecision = 0.0;
		double macroRecall = 0.0;
		double macroF1 = 0.0;
		double macroAuroc = 0.0;
		std::size_t macroClassCount = 0;
		std::size_t macroAurocClassCount = 0;
		for (const auto& [className, counts] : perClass)
		{
			const auto [precision, recall, f1] = metricsFor(counts);
			std::cout << "CLASS " << className << " - Queries: " << counts.queries << " TP: " << counts.tp << " FP: " << counts.fp
					  << " FN: " << counts.fn << " Precision: " << precision << " Recall: " << recall << " F1: " << f1 << " AUROC: ";
			if (counts.aurocQueries > 0)
			{
				const auto classAuroc = counts.auroc / static_cast<double>(counts.aurocQueries);
				std::cout << classAuroc;
				macroAuroc += classAuroc;
				macroAurocClassCount++;
			}
			else
				std::cout << "N/A";
			std::cout << std::endl;
			macroPrecision += precision;
			macroRecall += recall;
			macroF1 += f1;
			macroClassCount++;
		}
		if (macroClassCount > 0)
		{
			std::cout << "MACRO AVG - Precision: " << macroPrecision / static_cast<double>(macroClassCount)
					  << " Recall: " << macroRecall / static_cast<double>(macroClassCount)
					  << " F1: " << macroF1 / static_cast<double>(macroClassCount) << " AUROC: ";
			if (macroAurocClassCount > 0)
				std::cout << macroAuroc / static_cast<double>(macroAurocClassCount);
			else
				std::cout << "N/A";
			std::cout << std::endl;
		}
		const auto [precision, recall, f1] = metricsFor(total);
		std::cout << "TOTAL (micro) - TP: " << total.tp << " FP: " << total.fp << " FN: " << total.fn << " Precision: " << precision
				  << " Recall: " << recall << " F1: " << f1 << " Mean AUROC: ";
		if (totalAurocQueries > 0)
			std::cout << totalAuroc / static_cast<double>(totalAurocQueries);
		else
			std::cout << "N/A";
		std::cout << std::endl;

		if (argc == 7)
		{
			rocCsv.flush();
			scoreCsv.flush();
			if (!rocCsv || !scoreCsv)
			{
				std::cerr << "Failed while writing ROC CSV output: " << argv[6] << " or " << scoreCsvPath.string() << std::endl;
				return 1;
			}
			std::cout << "ROC curve data written to " << argv[6] << " and candidate scores to " << scoreCsvPath.string() << std::endl;
		}

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
