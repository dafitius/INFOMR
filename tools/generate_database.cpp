#define USE_MESHFIX 1

#include "generate_database.h"
#include "../src/geometry/mesh.hpp"
#include "../src/geometry/pipeline.hpp"
#include "../src/io/shape_db.hpp"
#include "../src/querying/feature_vector.hpp"
#include "../src/stats/distributions.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#if USE_MESHFIX
#include <meshfix.h>

#include <atomic>
#include <chrono>
#include <memory>
#endif

int main(int argc, char* argv[])
{
	std::uint32_t seed = querying::defaultSeed;
	if (argc > 2)
	{
		std::cerr << "Usage: generate_database [seed]" << std::endl;
		return 1;
	}

	if (argc > 1)
	{
		const std::string_view argument = argv[1];
		const auto result = std::from_chars(argument.data(), argument.data() + argument.size(), seed);
		if (result.ec != std::errc{} || result.ptr != argument.data() + argument.size())
		{
			std::cerr << "Usage: generate_database [seed]" << std::endl;
			return 1;
		}
	}

	try
	{
		return createCacheDB(seed);
	}
	catch (const std::exception& exception)
	{
		std::cerr << "Failed to create cache database: " << exception.what() << std::endl;
		return 1;
	}
}

struct Candidate
{
	std::filesystem::path path;
	Mesh mesh;
	querying::FeatureVector features;
};

#if USE_MESHFIX
namespace
{
	class TemporaryDirectory
	{
	public:
		TemporaryDirectory()
		{
			static std::atomic<std::uint64_t> sequence{0};
			const auto base = std::filesystem::temp_directory_path();
			const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();

			for (;;)
			{
				path_ = base / std::format(
					"generate_database_meshfix_{}_{}", timestamp, sequence.fetch_add(1));

				std::error_code error;
				if (std::filesystem::create_directory(path_, error))
					return;

				if (error)
					throw std::filesystem::filesystem_error(
						"Could not create MeshFix temporary directory", path_, error);
			}
		}

		TemporaryDirectory(const TemporaryDirectory&) = delete;
		TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

		~TemporaryDirectory()
		{
			std::error_code error;
			std::filesystem::remove_all(path_, error);
			if (error)
				std::cerr << "Could not remove temporary directory " << path_
				          << ": " << error.message() << '\n';
		}

		const std::filesystem::path& path() const
		{
			return path_;
		}

	private:
		std::filesystem::path path_;
	};

	bool isUsableMesh(const Mesh& mesh)
	{
		if (mesh.numFaces() == 0 || !mesh.isTriangleMesh())
			return false;

		const auto& topology = mesh.topology();
		if (!topology.isClosed() || !topology.nonManifoldEdges().empty())
			return false;

		for (std::size_t vertex = 0; vertex < mesh.numVertices(); ++vertex)
		{
			if (topology.vertexStarSubsets(vertex).size() > 1)
				return false;
		}

		const auto incidence = mesh.faces().edgeIncidence();
		for (const auto& occurrences : incidence | std::views::values)
		{
			if (occurrences.size() != 2)
				return false;

			const auto& a = occurrences[0];
			const auto& b = occurrences[1];
			if (a.face == b.face || a.from != b.to || a.to != b.from)
				return false;
		}

		return true;
	}

	std::optional<Mesh> repairWithMeshFix(const Mesh& input)
	{
		TemporaryDirectory temp;
		const auto inputPath = temp.path() / "input.obj";
		const auto outputPath = temp.path() / "repaired.obj";

		auto triangulated = input.triangulated();
		if (triangulated.numFaces() == 0)
		{
			std::cerr << "Triangulation produced an empty mesh\n";
			return std::nullopt;
		}
		triangulated.exportObj(inputPath);

		meshfix_mesh* inputRaw = nullptr;
		auto status = meshfix_mesh_read(inputPath.string().c_str(), &inputRaw);
		std::unique_ptr<meshfix_mesh, decltype(&meshfix_mesh_free)> inputMesh(
			inputRaw, &meshfix_mesh_free);

		if (status != MESHFIX_OK)
			throw std::runtime_error(std::format(
				"MeshFix could not read input: {}", meshfix_last_error()));

		meshfix_options options;
		meshfix_options_init(&options);

		meshfix_report report;
		meshfix_report_init(&report);

		meshfix_mesh* repairedRaw = nullptr;
		status = meshfix_repair(inputMesh.get(), &options, &repairedRaw, &report);
		std::unique_ptr<meshfix_mesh, decltype(&meshfix_mesh_free)> repairedMesh(
			repairedRaw, &meshfix_mesh_free);

		if (status != MESHFIX_OK)
			throw std::runtime_error(std::format(
				"MeshFix repair failed: {}", meshfix_last_error()));

		if (!report.manifold_after || !report.watertight_after)
		{
			std::cerr << "MeshFix could not fully repair mesh: "
			          << report.non_manifold_edges_after << " non-manifold edges, "
			          << report.boundary_edges_after << " boundary edges\n";
			return std::nullopt;
		}

		status = meshfix_mesh_write(repairedMesh.get(), outputPath.string().c_str());
		if (status != MESHFIX_OK)
			throw std::runtime_error(std::format(
				"MeshFix could not write output: {}", meshfix_last_error()));

		auto repaired = Mesh::fromObj(outputPath);
		if (!repaired)
			throw std::runtime_error("Could not load MeshFix output OBJ");

		if (!isUsableMesh(*repaired))
		{
			std::cerr << "Repaired mesh has invalid or inconsistent topology\n";
			return std::nullopt;
		}

		return std::move(*repaired);
	}
} // namespace
#endif

bool writeFeatures(const std::filesystem::path& outputPath, const std::vector<Candidate>& candidates)
{
	std::ofstream file(outputPath);
	if (!file)
		return false;

	file << "class,file";
	for (const auto name : querying::globalFeatureNames)
		file << ',' << name;

	for (const auto& distribution : stats::distributions())
	{
		for (std::size_t bin = 0; bin < querying::histogramBinCount; ++bin)
			file << std::format(",{}_{}", distribution.name, bin);
	}
	file << '\n';

	std::vector<const Candidate*> sorted;
	sorted.reserve(candidates.size());
	for (const auto& candidate : candidates)
		sorted.push_back(&candidate);

	std::ranges::sort(sorted, {}, &Candidate::path);

	for (const auto* candidatePtr : sorted)
	{
		const auto& candidate = *candidatePtr;
		if (candidate.mesh.numFaces() == 0)
		{
			std::cerr << "Skipped empty shape in features: " << candidate.path << '\n';
			continue;
		}

		file << candidate.path.parent_path().filename().string()
		     << ',' << candidate.path.filename().string();

		for (const auto value : candidate.features.global)
			file << std::format(",{}", value);

		for (const auto& histogram : candidate.features.histograms)
		{
			for (const auto value : histogram)
				file << std::format(",{}", value);
		}
		file << '\n';
	}

	file.close();
	return !file.fail();
}

std::optional<Mesh> normalizeShape(const std::filesystem::path& path)
{
	auto loaded = Mesh::fromObj(path);
	if (!loaded || loaded->numFaces() == 0)
	{
		std::cerr << "Could not load a non-empty mesh: " << path << '\n';
		return std::nullopt;
	}

#if USE_MESHFIX
	auto repaired = repairWithMeshFix(*loaded);
	if (!repaired)
		return std::nullopt;

	auto remeshed = repaired->remeshed(10000);
	if (!isUsableMesh(remeshed))
	{
		std::cerr << "Remeshing produced invalid topology: " << path << '\n';
		return std::nullopt;
	}

	auto remaining = mesh::Pipeline::parse("center > align_axes > flip object > scale_to_unit");
	if (!remaining)
		throw std::runtime_error("Invalid normalization pipeline");

	auto normalized = remaining->apply(remeshed);
	if (!isUsableMesh(normalized))
	{
		std::cerr << "Normalization produced invalid topology: " << path << '\n';
		return std::nullopt;
	}

	return normalized;
#else
	auto normalized = mesh::normalized(*loaded);
	if (normalized.numFaces() == 0)
	{
		std::cerr << "Normalization produced an empty mesh: " << path << '\n';
		return std::nullopt;
	}

	return normalized;
#endif
}

int createCacheDB(const std::uint32_t seed)
{
	const std::filesystem::path root{SHAPE_DB_ROOT};
	const std::filesystem::path cacheRoot{CACHED_DB_ROOT};

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
		std::cerr << "Failed to scan shape database directory: " << root.string()
		          << " (" << error.message() << ")" << std::endl;
		return 1;
	}

	std::size_t skipped = 0;

	while (it != end)
	{
		const auto path = it->path();

		if (it->is_regular_file(error) && !error && path.extension() == ".obj")
		{
			auto candidateShape = normalizeShape(path);
			if (!candidateShape)
			{
				std::cerr << "Skipped broken mesh: " << path << '\n';
				++skipped;
			}
			else
			{
				auto features = querying::extractFeatures(
					*candidateShape, querying::shapeSeed(path.filename().string(), seed));
				std::cout << "Processed " << path << std::endl;
				candidates.push_back({path, std::move(*candidateShape), std::move(features)});
			}
		}
		else if (error)
		{
			std::cerr << "Failed to inspect shape database entry: " << path.string()
			          << " (" << error.message() << ")" << std::endl;
			error.clear();
		}

		it.increment(error);
		if (error)
		{
			std::cerr << "Failed while scanning shape database: " << error.message() << std::endl;
			return 1;
		}
	}

	std::filesystem::create_directories(cacheRoot, error);
	if (error)
	{
		std::cerr << "Failed to create cache directory: " << cacheRoot.string()
		          << " (" << error.message() << ")" << std::endl;
		return 1;
	}

	for (const auto& candidate : candidates)
	{
		const auto relativePath = std::filesystem::relative(candidate.path, root, error);
		if (error)
		{
			std::cerr << "Failed to determine cache path for shape: " << candidate.path.string()
			          << " (" << error.message() << ")" << std::endl;
			return 1;
		}

		const auto outputPath = cacheRoot / relativePath;
		std::filesystem::create_directories(outputPath.parent_path(), error);
		if (error)
		{
			std::cerr << "Failed to create cache directory: " << outputPath.parent_path().string()
			          << " (" << error.message() << ")" << std::endl;
			return 1;
		}

		candidate.mesh.exportObj(outputPath);
		std::cout << "Exported " << outputPath << std::endl;
	}

	const auto featuresPath = cacheRoot /
		(seed == querying::defaultSeed ? "features.csv" : std::format("features_seed{}.csv", seed));

	if (!writeFeatures(featuresPath, candidates))
	{
		std::cerr << "Failed to write features: " << featuresPath.string() << std::endl;
		return 1;
	}

	std::cout << "Wrote features to " << featuresPath.string() << std::endl;
	std::cout << "Successfully created cache DB: " << candidates.size()
	          << " processed, " << skipped << " skipped" << std::endl;
	return 0;
}