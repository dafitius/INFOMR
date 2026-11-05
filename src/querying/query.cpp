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

	struct ShapeData
	{
		Mesh mesh;
		ShapeDescriptor descriptor;
	};

	struct Candidate
	{
		std::filesystem::path path;
		ShapeData shape;
	};

	std::optional<ShapeData> getShapeData(const std::filesystem::path& path, const bool alreadyNormalized = false)
	{
		auto loaded = Mesh::fromObj(path);
		if (!loaded)
		{
			std::cerr << "Could not load mesh" << std::endl;
			return std::nullopt;
		}

		auto mesh = alreadyNormalized ? std::move(*loaded) : mesh::normalized(*loaded);
		auto descriptor = querying::extractFeatures(mesh, querying::shapeSeed(path.filename().string()));

		if (!std::ranges::all_of(descriptor.global, [](const double value) { return std::isfinite(value); }))
		{
			std::cerr << "A global descriptor is non-finite:" << std::endl;
			for (std::size_t i = 0; i < descriptor.global.size(); ++i)
				std::cerr << "  " << querying::globalFeatureNames[i] << ": " << descriptor.global[i] << std::endl;
			return std::nullopt;
		}

		return ShapeData{std::move(mesh), std::move(descriptor)};
	}

	void normalizeGlobalDescriptors(const std::vector<ShapeDescriptor*>& descriptors)
	{
		if (descriptors.empty())
			return;

		for (std::size_t feature = 0; feature < globalFeatureCount; ++feature)
		{
			double mean = 0.0;
			for (const auto* descriptor : descriptors)
				mean += descriptor->global[feature];
			mean /= static_cast<double>(descriptors.size());

			double variance = 0.0;
			for (const auto* descriptor : descriptors)
			{
				const double difference = descriptor->global[feature] - mean;
				variance += difference * difference;
			}
			variance /= static_cast<double>(descriptors.size());

			if (variance == 0.0)
			{
				for (auto* descriptor : descriptors)
					descriptor->global[feature] = 0.0;
				continue;
			}

			const double standardDeviation = std::sqrt(variance);
			double maxAbsoluteZScore = 0.0;
			for (const auto* descriptor : descriptors)
				maxAbsoluteZScore = std::max(maxAbsoluteZScore, std::abs((descriptor->global[feature] - mean) / standardDeviation));

			for (auto* descriptor : descriptors)
				descriptor->global[feature] = (descriptor->global[feature] - mean) / (standardDeviation * maxAbsoluteZScore);
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

	auto queryShape = getShapeData(queryMesh);
	if (!queryShape)
	{
		std::cerr << "Failed to calculate descriptors for query mesh: " << queryMesh.string() << std::endl;
		return {};
	}

#ifdef SHAPE_DB_ROOT
	const std::filesystem::path root{SHAPE_DB_ROOT};
#else
	std::cerr << "Shape database root is not configured." << std::endl;
	return {};
#endif

#ifdef CACHED_DB_ROOT
	const std::filesystem::path cacheRoot{CACHED_DB_ROOT};
#endif

	std::error_code error;
	if (!std::filesystem::is_directory(root, error))
	{
		std::cerr << "Shape database directory is unavailable: " << root.string();
		if (error)
			std::cerr << " (" << error.message() << ')';
		std::cerr << '\n';
		return {};
	}

	const auto normalizedQueryPath = std::filesystem::weakly_canonical(queryMesh, error);
	if (error)
		error.clear();

	std::vector<Candidate> candidates;
	std::filesystem::recursive_directory_iterator it(root, error);
	const std::filesystem::recursive_directory_iterator end;
	if (error)
	{
		std::cerr << "Failed to scan shape database directory: " << root.string() << " (" << error.message() << ")\n";
		return {};
	}

	while (it != end)
	{
		const auto path = it->path();

		if (it->is_regular_file(error) && !error && path.extension() == ".obj")
		{
			auto meshPath = path;
			bool alreadyNormalized = false;
#ifdef CACHED_DB_ROOT
			const auto relativePath = path.lexically_relative(root);
			if (!relativePath.empty())
			{
				const auto cachedPath = cacheRoot / relativePath;
				error.clear();
				if (std::filesystem::is_regular_file(cachedPath, error))
				{
					meshPath = cachedPath;
					alreadyNormalized = true;
				}
				else if (error)
				{
					std::cerr << "Failed to inspect cached mesh: " << cachedPath.string() << " (" << error.message() << ")\n";
					error.clear();
				}
			}
#endif

			error.clear();
			const auto normalizedCandidatePath = std::filesystem::weakly_canonical(path, error);
			error.clear();
			const auto normalizedMeshPath = std::filesystem::weakly_canonical(meshPath, error);
			if (error || (normalizedCandidatePath != normalizedQueryPath && normalizedMeshPath != normalizedQueryPath))
			{
				error.clear();
				if (auto candidateShape = getShapeData(meshPath, alreadyNormalized))
				{
					candidates.push_back({meshPath, std::move(*candidateShape)});
				}
			}
		}
		else if (error)
		{
			std::cerr << "Failed to inspect shape database entry: " << path.string() << " (" << error.message() << ")\n";
			error.clear();
		}

		it.increment(error);
		if (error)
		{
			std::cerr << "Failed while scanning shape database: " << error.message() << '\n';
			return {};
		}
	}

	std::vector<ShapeDescriptor*> descriptors;
	descriptors.reserve(candidates.size() + 1);
	descriptors.push_back(&queryShape->descriptor);
	for (auto& candidate : candidates)
		descriptors.push_back(&candidate.shape.descriptor);
	normalizeGlobalDescriptors(descriptors);

	std::vector<std::pair<double, std::size_t>> ranked;
	for (std::size_t i = 0; i < candidates.size(); ++i)
	{
		const auto& candidate = candidates[i];
		if (const auto distance = descriptorDistance(queryShape->descriptor, candidate.shape.descriptor, weights))
			ranked.emplace_back(*distance, i);
	}

	std::sort(
		ranked.begin(), ranked.end(),
		[&candidates](const auto& a, const auto& b)
		{
			if (a.first != b.first)
				return a.first < b.first;
			return candidates[a.second].path < candidates[b.second].path;
		});

	std::vector<QueryResult> results;
	results.reserve(std::min(topK, ranked.size()));

	for (std::size_t i = 0; i < std::min(topK, ranked.size()); ++i)
	{
		const auto [distance, candidateIndex] = ranked[i];
		const auto& candidate = candidates[candidateIndex];
		results.emplace_back(candidate.path.filename().string(), candidate.path, distance, std::make_shared<Mesh>(candidate.shape.mesh));
	}

	std::chrono::steady_clock::time_point endTime = std::chrono::steady_clock::now();
	std::cout << "Calculation done in " << std::chrono::duration_cast<std::chrono::seconds>(endTime - begin).count() << "[seconds]" << std::endl;

	return results;
}
