#include "generate_database.h"
#include "../src/geometry/mesh.hpp"
#include "../src/io/shape_db.hpp"

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
	constexpr std::size_t targetVertexCount = 10000;
}

int main(int argc, char* argv[])
{
	(void)argc;
	(void)argv;

	return createCacheDB();
}

struct Candidate
{
	std::filesystem::path path;
	Mesh mesh;
};

Mesh normalizeShape(const std::filesystem::path& path)
{
	auto loaded = Mesh::fromObj(path);
	if (!loaded || !loaded.has_value())
	{
		std::cerr << "Could not load mesh" << std::endl;
		return Mesh{};
	}

	auto mesh = loaded.value();

	auto step1 = mesh.withConsistentOrientations()
					 .triangulated()
					 .removeNonManifold()
					 .withConsistentOrientations()
					 .filledHoles()
					 .remeshed(targetVertexCount)
					 .translated(-mesh.baryCenter())
					 .rotated(glm::inverse(mesh.principleAxes()));


	auto step2 = step1.moment().x < 0 ? step1.rotated(glm::quat{0, 0, 0, 1}) : Mesh::copy(step1);

	auto bounds = step2.bounds();
	auto step3 = step2.scaled(glm::vec3{1 / (bounds.max.x - bounds.min.x)});
	return step3;
}

int createCacheDB()
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
			std::cout << "Processed " << path << std::endl;
			candidates.push_back({path, std::move(candidateShape)});
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

	std::cout << "Succesfully created cacheDb" << std::endl;
	return 0;
}
