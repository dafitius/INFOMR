#include "query.h"

#include "../geometry/mesh.hpp"
#include "../geometry/pipeline.hpp"
#include "../stats/distributions.hpp"
#include "feature_tree.hpp"
#include "feature_vector.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace
{
	using querying::globalFeatureCount;
	using ShapeDescriptor = querying::FeatureVector;

	std::optional<ShapeDescriptor> computeQueryFeatures(const std::filesystem::path& path)
	{
		auto loaded = Mesh::fromObj(path);
		if (!loaded)
		{
			std::cerr << "Could not load mesh" << std::endl;
			return std::nullopt;
		}

		const auto mesh = mesh::normalized(*loaded);
		auto descriptor = querying::extractFeatures(mesh, querying::shapeSeed(path.filename().string()));

		if (!std::ranges::all_of(descriptor.global, [](const double value) { return std::isfinite(value); }))
		{
			std::cerr << "A global descriptor is non-finite:" << std::endl;
			for (std::size_t i = 0; i < descriptor.global.size(); ++i)
				std::cerr << "  " << querying::globalFeatureNames[i] << ": " << descriptor.global[i] << std::endl;
			return std::nullopt;
		}

		return descriptor;
	}

	std::optional<std::size_t> findDatabaseShape(const std::vector<querying::StoredShape>& database, const std::filesystem::path& queryPath)
	{
		std::error_code error;
		const auto databaseRoot = std::filesystem::weakly_canonical(queryPath, error).parent_path().parent_path();
		if (error || (databaseRoot != std::filesystem::weakly_canonical(SHAPE_DB_ROOT, error) &&
					  databaseRoot != std::filesystem::weakly_canonical(CACHED_DB_ROOT, error)))
			return std::nullopt;

		const auto className = queryPath.parent_path().filename().string();
		const auto fileName = queryPath.filename().string();
		const auto found = std::ranges::find_if(
			database, [&](const querying::StoredShape& shape) { return shape.className == className && shape.fileName == fileName; });
		if (found == database.end())
			return std::nullopt;
		return static_cast<std::size_t>(found - database.begin());
	}

	struct Database
	{
		std::vector<querying::StoredShape> shapes;
		std::array<double, globalFeatureCount> mean{};
		std::array<double, globalFeatureCount> scale{};

		void standardize(ShapeDescriptor& descriptor) const
		{
			for (std::size_t feature = 0; feature < globalFeatureCount; ++feature)
				descriptor.global[feature] = scale[feature] == 0.0 ? 0.0 : (descriptor.global[feature] - mean[feature]) / scale[feature];
		}
	};

	Database createDatabase(std::vector<querying::StoredShape> shapes)
	{
		Database database{.shapes = std::move(shapes), .mean = {}, .scale = {}};
		if (database.shapes.empty())
			return database;

		for (std::size_t feature = 0; feature < globalFeatureCount; ++feature)
		{
			double mean = 0.0;
			for (const auto& shape : database.shapes)
				mean += shape.features.global[feature];
			mean /= static_cast<double>(database.shapes.size());

			double variance = 0.0;
			for (const auto& shape : database.shapes)
			{
				const double difference = shape.features.global[feature] - mean;
				variance += difference * difference;
			}
			variance /= static_cast<double>(database.shapes.size());

			database.mean[feature] = mean;
			if (variance == 0.0)
				continue;

			const double standardDeviation = std::sqrt(variance);
			double maxAbsoluteZScore = 0.0;
			for (const auto& shape : database.shapes)
				maxAbsoluteZScore = std::max(maxAbsoluteZScore, std::abs((shape.features.global[feature] - mean) / standardDeviation));
			database.scale[feature] = standardDeviation * maxAbsoluteZScore;
		}

		for (auto& shape : database.shapes)
			database.standardize(shape.features);
		return database;
	}

	const Database* database()
	{
		static std::optional<Database> cached;
		if (!cached)
		{
			if (auto shapes = querying::loadFeatures(std::filesystem::path(CACHED_DB_ROOT) / "features.csv"))
				cached = createDatabase(std::move(*shapes));
		}
		return cached ? &*cached : nullptr;
	}

	const querying::FeatureTree& featureTree(
		const Database& database, const std::vector<double>& weights, const querying::TreeMetric metric, const bool withHistograms)
	{
		static std::unique_ptr<querying::FeatureTree> tree;
		if (!tree || tree->metric() != metric || tree->withHistograms() != withHistograms || tree->weights() != weights)
		{
			const auto begin = std::chrono::steady_clock::now();
			tree = std::make_unique<querying::FeatureTree>(database.shapes, weights, metric, withHistograms);
			std::cout << "Built the KD-tree in "
					  << std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count() << " ms" << std::endl;
		}
		return *tree;
	}

	double weightAt(const std::vector<double>& weights, const std::size_t index)
	{
		return querying::featureWeight(weights, index);
	}

	std::optional<double> descriptorDistance(const ShapeDescriptor& query, const ShapeDescriptor& candidate, const std::vector<double>& weights)
	{
		if (query.histograms.size() != candidate.histograms.size())
			return std::nullopt;

		double globalSquaredDistance = 0.0;
		double globalWeight = 0.0;

		for (std::size_t i = 0; i < globalFeatureCount; ++i)
		{
			const double weight = weightAt(weights, i);
			const double difference = query.global[i] - candidate.global[i];
			globalSquaredDistance += weight * difference * difference;
			globalWeight += weight;
		}

		double result = 0.0;
		double resultWeight = 0.0;

		if (globalWeight != 0.0)
		{
			result += std::sqrt(globalSquaredDistance / globalWeight) * globalWeight;
			resultWeight += globalWeight;
		}

		double distributionDistance = 0.0;
		double distributionWeight = 0.0;

		for (std::size_t i = 0; i < query.histograms.size(); ++i)
		{
			const auto& queryHistogram = query.histograms[i];
			const auto& candidateHistogram = candidate.histograms[i];
			const double weight = weightAt(weights, globalFeatureCount + i);
			const auto dist = stats::earthMoversDistance(queryHistogram, candidateHistogram);

			if (dist.has_value())
			{
				distributionDistance += weight * dist.value();
				distributionWeight += weight;
			}
		}

		if (distributionWeight != 0.0)
		{
			result += distributionDistance;
			resultWeight += distributionWeight;
		}

		if (resultWeight == 0.0)
			return std::nullopt;

		return result / resultWeight;
	}

	double binDistance(const ShapeDescriptor& query, const ShapeDescriptor& candidate, const std::vector<double>& weights, const bool l1)
	{
		const auto lp = [l1](const double difference) { return l1 ? std::abs(difference) : difference * difference; };
		const auto root = [l1](const double sum) { return l1 ? sum : std::sqrt(sum); };

		double globalDistance = 0.0;
		double globalWeight = 0.0;
		for (std::size_t i = 0; i < globalFeatureCount; ++i)
		{
			globalDistance += weightAt(weights, i) * lp(query.global[i] - candidate.global[i]);
			globalWeight += weightAt(weights, i);
		}

		double histogramDistance = 0.0;
		double histogramWeight = 0.0;
		for (std::size_t h = 0; h < query.histograms.size(); ++h)
		{
			double sum = 0.0;
			for (std::size_t bin = 0; bin < query.histograms[h].size(); ++bin)
				sum += lp(query.histograms[h][bin] - candidate.histograms[h][bin]);
			histogramDistance += weightAt(weights, globalFeatureCount + h) * root(sum / 2.0);
			histogramWeight += weightAt(weights, globalFeatureCount + h);
		}

		double result = 0.0;
		int parts = 0;
		if (globalWeight > 0.0)
		{
			result += root(globalDistance / globalWeight);
			++parts;
		}
		if (histogramWeight > 0.0)
		{
			result += histogramDistance / histogramWeight;
			++parts;
		}
		return parts == 0 ? 0.0 : result / parts;
	}
} // namespace

Query::Query(std::filesystem::path queryMesh, std::vector<double> weights, const DistanceMode mode)
	: queryMesh(std::move(queryMesh))
	, weights(std::move(weights))
	, mode(mode)
{
	for (double& weight : this->weights)
		if (!std::isfinite(weight) || weight < 0.0)
			weight = 0.0;
}

std::vector<QueryResult> Query::search(std::size_t topK, const bool loadMeshes)
{
	if (topK == 0)
		return {};

	const std::filesystem::path shapeRoot{SHAPE_DB_ROOT};
	const std::filesystem::path cacheRoot{CACHED_DB_ROOT};

	const auto* db = database();
	if (!db)
		return {};

	const auto queryIndex = findDatabaseShape(db->shapes, queryMesh);
	std::optional<ShapeDescriptor> queryDescriptor = queryIndex ? std::optional{db->shapes[*queryIndex].features} : computeQueryFeatures(queryMesh);
	if (!queryDescriptor)
	{
		std::cerr << "Failed to calculate descriptors for query mesh: " << queryMesh.string() << std::endl;
		return {};
	}
	if (!queryIndex)
		db->standardize(*queryDescriptor);

	std::vector<std::pair<double, std::size_t>> ranked;
	if (mode == DistanceMode::Mix || mode == DistanceMode::L1 || mode == DistanceMode::L2)
	{
		const auto begin = std::chrono::steady_clock::now();
		for (std::size_t i = 0; i < db->shapes.size(); ++i)
		{
			if (i == queryIndex)
				continue;
			const auto& candidate = db->shapes[i].features;
			const auto distance = mode == DistanceMode::Mix ? descriptorDistance(*queryDescriptor, candidate, weights)
															: binDistance(*queryDescriptor, candidate, weights, mode == DistanceMode::L1);
			if (distance)
				ranked.emplace_back(*distance, i);
		}
		std::ranges::stable_sort(ranked, {}, &std::pair<double, std::size_t>::first);
		std::cout << "Brute force search done in " << std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count()
				  << " ms" << std::endl;
	}
	else
	{
		const auto metric = mode == DistanceMode::GlobalsL2 ? querying::TreeMetric::L2 : querying::TreeMetric::L1;
		const auto& tree = featureTree(*db, weights, metric, mode == DistanceMode::L1Emd);
		const auto begin = std::chrono::steady_clock::now();

		ranked = tree.nearest(*queryDescriptor, topK + 1);
		const auto self = std::ranges::find(ranked, queryIndex, &std::pair<double, std::size_t>::second);
		if (self != ranked.end())
			ranked.erase(self);
		std::cout << "KD-tree search done in " << std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count() << " ms"
				  << std::endl;
	}

	std::vector<QueryResult> results;
	results.reserve(std::min(topK, ranked.size()));

	for (std::size_t i = 0; i < std::min(topK, ranked.size()); ++i)
	{
		const auto [distance, shapeIndex] = ranked[i];
		const auto& shape = db->shapes[shapeIndex];

		if (!loadMeshes)
		{
			results.emplace_back(shape.fileName, shape.className, shapeRoot / shape.className / shape.fileName, distance);
			continue;
		}

		const auto cachedPath = cacheRoot / shape.className / shape.fileName;
		if (auto mesh = Mesh::fromObj(cachedPath))
			results.emplace_back(shape.fileName, shape.className, cachedPath, distance, std::make_shared<Mesh>(std::move(*mesh)));
		else
			results.emplace_back(shape.fileName, shape.className, shapeRoot / shape.className / shape.fileName, distance);
	}

	return results;
}
