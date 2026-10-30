#include "query.h"

#include "../geometry/mesh.hpp"
#include "../stats/distributions.hpp"

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
	constexpr std::size_t globalDescriptorCount = 7;
	constexpr std::size_t histogramBinCount = 32;

	struct ShapeDescriptor
	{
		std::array<double, globalDescriptorCount> global;
		std::vector<std::vector<double>> distributions;
	};

	Mesh normalize(const Mesh& mesh)
	{
		auto step1 = mesh.withConsistentOrientations()
						 .triangulated()
						 .removeNonManifold()
						 .withConsistentOrientations()
						 .filledHoles()
						 //  .subdivided()
						 .translated(-mesh.baryCenter())
						 .rotated(glm::inverse(mesh.principleAxes()));


		auto step2 = step1.moment().x < 0 ? step1.rotated(glm::quat{0, 0, 0, 1}) : Mesh::copy(step1);

		auto bounds = step2.bounds();
		auto step3 = step2.scaled(glm::vec3{1 / (bounds.max.x - bounds.min.x)});
		return step3;
	}

	std::optional<ShapeDescriptor> getDescriptor(const std::filesystem::path& path)
	{
		auto loaded = Mesh::fromObj(path);
		if (!loaded)
			return std::nullopt;

		const auto mesh = normalize(*loaded);
		ShapeDescriptor descriptor{
			.global =
				{mesh.volume(), mesh.surfaceArea(), mesh.compactness(), mesh.rectangularity(), mesh.diameter(), mesh.convexity(),
				 mesh.eccentricity()},
			.distributions = {},
		};

		for (const auto& distribution : stats::distributions())
		{
			const auto samples = distribution.sample(mesh);
			descriptor.distributions.push_back(stats::binHistogram(samples, distribution.range, histogramBinCount));
		}

		if (!std::ranges::all_of(descriptor.global, [](const double value) { return std::isfinite(value); }))
			return std::nullopt;

		return descriptor;
	}

	double weightAt(const std::vector<double>& weights, const std::size_t index)
	{
		if (index >= weights.size())
			return 1.0;

		return std::isfinite(weights[index]) ? std::max(0.0, weights[index]) : 0.0;
	}

	std::optional<double> descriptorDistance(const ShapeDescriptor& query, const ShapeDescriptor& candidate, const std::vector<double>& weights)
	{
		if (query.distributions.size() != candidate.distributions.size())
			return std::nullopt;

		double weightedSquaredDistance = 0.0;
		double totalWeight = 0.0;

		for (std::size_t i = 0; i < globalDescriptorCount; ++i)
		{
			const double weight = weightAt(weights, i);
			const double difference = query.global[i] - candidate.global[i];
			weightedSquaredDistance += weight * difference * difference;
			totalWeight += weight;
		}

		for (std::size_t i = 0; i < query.distributions.size(); ++i)
		{
			const auto& queryHistogram = query.distributions[i];
			const auto& candidateHistogram = candidate.distributions[i];
			if (queryHistogram.size() != candidateHistogram.size())
				return std::nullopt;

			double squaredDistance = 0.0;
			for (std::size_t bin = 0; bin < queryHistogram.size(); ++bin)
			{
				const double difference = queryHistogram[bin] - candidateHistogram[bin];
				squaredDistance += difference * difference;
			}

			const double weight = weightAt(weights, globalDescriptorCount + i);
			weightedSquaredDistance += weight * squaredDistance / static_cast<double>(queryHistogram.size());
			totalWeight += weight;
		}

		if (totalWeight == 0.0)
			return std::nullopt;

		return std::sqrt(weightedSquaredDistance / totalWeight);
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

	const auto queryDescriptor = getDescriptor(queryMesh);
	if (!queryDescriptor)
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

	std::vector<std::pair<double, QueryResult>> ranked;
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
			error.clear();
			const auto normalizedCandidatePath = std::filesystem::weakly_canonical(path, error);
			if (error || normalizedCandidatePath != normalizedQueryPath)
			{
				error.clear();
				if (const auto candidateDescriptor = getDescriptor(path))
				{
					if (const auto distance = descriptorDistance(*queryDescriptor, *candidateDescriptor, weights))
					{
						ranked.emplace_back(*distance, QueryResult{path.filename().string(), path, *distance});
					}
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

	std::sort(
		ranked.begin(), ranked.end(),
		[](const auto& a, const auto& b)
		{
			if (a.first != b.first)
				return a.first < b.first;
			return a.second.meshPath < b.second.meshPath;
		});

	std::vector<QueryResult> results;
	results.reserve(std::min(topK, ranked.size()));

	for (std::size_t i = 0; i < std::min(topK, ranked.size()); ++i)
		results.push_back(std::move(ranked[i].second));

	std::chrono::steady_clock::time_point endTime = std::chrono::steady_clock::now();
	std::cout << "Calculation done in " << std::chrono::duration_cast<std::chrono::seconds>(endTime - begin).count() << "[seconds]" << std::endl;

	return results;
}
