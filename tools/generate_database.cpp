#include "generate_database.h"
#include "../src/geometry/mesh.hpp"
#include "../src/geometry/pipeline.hpp"
#include "../src/io/shape_db.hpp"
#include "../src/querying/feature_vector.hpp"
#include "../src/stats/distributions.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <format>
#include <fstream>
#include <iostream>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

int main(int argc, char* argv[])
{
	std::uint32_t seed = querying::defaultSeed;
	if (argc > 1)
	{
		const std::string_view argument = argv[1];
		if (std::from_chars(argument.data(), argument.data() + argument.size(), seed).ec != std::errc{})
		{
			std::cerr << "Usage: generate_database [seed]" << std::endl;
			return 1;
		}
	}

	return createCacheDB(seed);
}

struct Candidate
{
	std::filesystem::path path;
	Mesh mesh;
	querying::FeatureVector features;
};

bool writeFeatures(const std::filesystem::path& outputPath, const std::vector<Candidate>& candidates)
{
	std::ofstream file(outputPath);
	if (!file)
	{
		return false;
	}

	file << "class,file";
	for (const auto name : querying::globalFeatureNames)
	{
		file << ',' << name;
	}
	for (const auto& distribution : stats::distributions())
	{
		for (std::size_t bin = 0; bin < querying::histogramBinCount; ++bin)
		{
			file << std::format(",{}_{}", distribution.name, bin);
		}
	}
	file << '\n';

	std::vector<const Candidate*> sorted;
	for (const auto& candidate : candidates)
	{
		sorted.push_back(&candidate);
	}
	std::ranges::sort(sorted, {}, &Candidate::path);

	for (const auto* candidatePtr : sorted)
	{
		const auto& candidate = *candidatePtr;
		if (candidate.mesh.numFaces() == 0)
		{
			std::cerr << "Skipped empty shape in features: " << candidate.path.string() << std::endl;
			continue;
		}

		file << candidate.path.parent_path().filename().string() << ',' << candidate.path.filename().string();
		for (const auto value : candidate.features.global)
		{
			file << std::format(",{}", value);
		}
		for (const auto& histogram : candidate.features.histograms)
		{
			for (const auto value : histogram)
			{
				file << std::format(",{}", value);
			}
		}
		file << '\n';
	}
	return true;
}

Mesh normalizeShape(const std::filesystem::path& path)
{
	auto loaded = Mesh::fromObj(path);
	if (!loaded || !loaded.has_value())
	{
		std::cerr << "Could not load mesh" << std::endl;
		return Mesh{};
	}

	return mesh::normalized(loaded.value());
}

int createCacheDB(const std::uint32_t seed)
{
	const std::filesystem::path root{SHAPE_DB_ROOT};

	std::error_code error;
	if (!std::filesystem::is_directory(root, error))
	{
		std::cerr << "Shape database directory is unavailable: " << root.string();
		if (error)
			std::cerr << " (" << error.message() << ')';
		std::cerr << std::endl;
		return 1;
	}

	std::vector<Candidate> candidates;
	std::filesystem::recursive_directory_iterator it(root, error);
	const std::filesystem::recursive_directory_iterator end;

	if (error)
	{
		std::cerr << "Failed to scan shape database directory: " << root.string() << " (" << error.message() << ")" << std::endl;
		return 1;
	}


	while (it != end)
	{
		const auto path = it->path();

		if (it->is_regular_file(error) && !error && path.extension() == ".obj")
		{
			error.clear();
			auto candidateShape = normalizeShape(path);
			auto features = querying::extractFeatures(candidateShape, querying::shapeSeed(path.filename().string(), seed));
			std::cout << "Processed " << path << std::endl;
			candidates.push_back({path, std::move(candidateShape), std::move(features)});
		}
		else if (error)
		{
			std::cerr << "Failed to inspect shape database entry: " << path.string() << " (" << error.message() << ")" << std::endl;
			error.clear();
		}

		it.increment(error);
		if (error)
		{
			std::cerr << "Failed while scanning shape database: " << error.message() << std::endl;
			return 1;
		}
	}

	for (const auto& candidate : candidates)
	{
		auto relativePath = std::filesystem::relative(candidate.path, SHAPE_DB_ROOT, error);
		if (error)
		{
			std::cerr << "Failed to determine cache path for shape: " << candidate.path.string() << " (" << error.message() << ")" << std::endl;
			return 1;
		}

		const auto outputPath = std::filesystem::path(CACHED_DB_ROOT) / relativePath;
		std::filesystem::create_directories(outputPath.parent_path(), error);
		if (error)
		{
			std::cerr << "Failed to create cache directory: " << outputPath.parent_path().string() << " (" << error.message() << ")" << std::endl;
			return 1;
		}

		candidate.mesh.exportObj(outputPath);
		std::cout << "Exported " << outputPath << std::endl;
	}

	const auto featuresPath =
		std::filesystem::path(CACHED_DB_ROOT) / (seed == querying::defaultSeed ? "features.csv" : std::format("features_seed{}.csv", seed));
	if (!writeFeatures(featuresPath, candidates))
	{
		std::cerr << "Failed to write features: " << featuresPath.string() << std::endl;
		return 1;
	}
	std::cout << "Wrote features to " << featuresPath.string() << std::endl;

	std::cout << "Succesfully created cacheDb" << std::endl;
	return 0;
}
