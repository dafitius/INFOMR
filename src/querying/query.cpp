#include "query.h"

#include "../geometry/mesh.hpp"
#include "../geometry/pipeline.hpp"
#include "../stats/distributions.hpp"
#include "feature_vector.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
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
		if (error ||
			(databaseRoot != std::filesystem::weakly_canonical(SHAPE_DB_ROOT, error) &&
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

	void standardizeGlobalDescriptors(std::vector<querying::StoredShape>& database, ShapeDescriptor& query)
	{
		if (database.empty())
			return;

		for (std::size_t feature = 0; feature < globalFeatureCount; ++feature)
		{
			double mean = 0.0;
			for (const auto& shape : database)
				mean += shape.features.global[feature];
			mean /= static_cast<double>(database.size());

			double variance = 0.0;
			for (const auto& shape : database)
			{
				const double difference = shape.features.global[feature] - mean;
				variance += difference * difference;
			}
			variance /= static_cast<double>(database.size());

			if (variance == 0.0)
			{
				for (auto& shape : database)
					shape.features.global[feature] = 0.0;
				query.global[feature] = 0.0;
				continue;
			}

			const double standardDeviation = std::sqrt(variance);
			double maxAbsoluteZScore = 0.0;
			for (const auto& shape : database)
				maxAbsoluteZScore = std::max(maxAbsoluteZScore, std::abs((shape.features.global[feature] - mean) / standardDeviation));

			for (auto& shape : database)
				shape.features.global[feature] = (shape.features.global[feature] - mean) / (standardDeviation * maxAbsoluteZScore);
			query.global[feature] = (query.global[feature] - mean) / (standardDeviation * maxAbsoluteZScore);
		}
	}

	double weightAt(const std::vector<double>& weights, const std::size_t index)
	{
		if (index >= weights.size())
			return 1.0;

		return std::isfinite(weights[index]) ? std::max(0.0, weights[index]) : 0.0;
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

		auto result = 0.0;
		auto resultCount = 0;

		if (globalWeight != 0.0)
		{
			result += std::sqrt(globalSquaredDistance / globalWeight);
			resultCount++;
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
			result += distributionDistance / distributionWeight;
			resultCount++;
		}

		if (resultCount == 0)
			return std::nullopt;

		return result / resultCount;
	}
} // namespace

Query::Query(std::filesystem::path queryMesh, std::vector<double> weights)
	: queryMesh(std::move(queryMesh))
	, weights(std::move(weights))
{
}

std::vector<QueryResult> Query::search(std::size_t topK)
{
	std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
	if (topK == 0)
		return {};

	const std::filesystem::path shapeRoot{SHAPE_DB_ROOT};
	const std::filesystem::path cacheRoot{CACHED_DB_ROOT};

	auto database = querying::loadFeatures(cacheRoot / "features.csv");
	if (!database)
		return {};

	const auto queryIndex = findDatabaseShape(*database, queryMesh);
	std::optional<ShapeDescriptor> queryDescriptor =
		queryIndex ? std::optional{(*database)[*queryIndex].features} : computeQueryFeatures(queryMesh);
	if (!queryDescriptor)
	{
		std::cerr << "Failed to calculate descriptors for query mesh: " << queryMesh.string() << std::endl;
		return {};
	}

	standardizeGlobalDescriptors(*database, *queryDescriptor);

	std::vector<std::pair<double, std::size_t>> ranked;
	for (std::size_t i = 0; i < database->size(); ++i)
	{
		if (i == queryIndex)
			continue;
		if (const auto distance = descriptorDistance(*queryDescriptor, (*database)[i].features, weights))
			ranked.emplace_back(*distance, i);
	}

	std::ranges::stable_sort(ranked, {}, &std::pair<double, std::size_t>::first);

	std::vector<QueryResult> results;
	results.reserve(std::min(topK, ranked.size()));

	for (std::size_t i = 0; i < std::min(topK, ranked.size()); ++i)
	{
		const auto [distance, shapeIndex] = ranked[i];
		const auto& shape = (*database)[shapeIndex];

		const auto cachedPath = cacheRoot / shape.className / shape.fileName;
		if (auto mesh = Mesh::fromObj(cachedPath))
			results.emplace_back(shape.fileName, cachedPath, distance, std::make_shared<Mesh>(std::move(*mesh)));
		else
			results.emplace_back(shape.fileName, shapeRoot / shape.className / shape.fileName, distance);
	}

	std::chrono::steady_clock::time_point endTime = std::chrono::steady_clock::now();
	std::cout << "Calculation done in " << std::chrono::duration_cast<std::chrono::milliseconds>(endTime - begin).count() << " ms" << std::endl;

	return results;
}
