//
// Created by dafitius on 17-09-2026.
//

#ifndef MR_SHAPE_DATA_ACCESS_H
#define MR_SHAPE_DATA_ACCESS_H
#include "../geometry/mesh.hpp"
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <format>
#include <iostream>
#include <optional>
#include <random>
#include <ranges>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

struct ShapeRef
{
	std::string className;
	std::string fileName;
};

class ShapeDb
{
	std::filesystem::path shape_database_path;
	std::unordered_map<std::string, std::vector<std::string>> entries_;

public:
	static std::optional<ShapeDb> open(const std::filesystem::path& shapeDatabasePath)
	{
		auto self = ShapeDb();

		if (!std::filesystem::exists(shapeDatabasePath))
		{
			std::cerr << "Unable to open file!" << shapeDatabasePath.string() << '\n';
			return std::nullopt;
		}

		auto entries = std::filesystem::directory_iterator(shapeDatabasePath) |
					   std::views::filter([](const auto& item) { return item.is_directory(); }) |
					   std::views::transform(
						   [&](const auto& item)
						   {
							   auto folderNames = std::vector<std::string>();
							   auto view = std::filesystem::directory_iterator(item.path()) |
										   std::views::filter([](const auto& file) { return file.is_regular_file(); }) |
										   std::views::transform([&](const auto& file) { return file.path().filename().string(); });

							   std::ranges::copy(view, std::back_inserter(folderNames));
							   return std::pair<std::string, std::vector<std::string>>(item.path().filename().string(), std::move(folderNames));
						   });

		std::ranges::for_each(entries, [&self](const auto& entry) { self.entries_.emplace(entry.first, entry.second); });

		self.shape_database_path = shapeDatabasePath;

		return self;
	}

	auto iterClassNames() const
	{
		return entries_ | std::views::keys;
	}

	auto iterAllShapePaths() const
	{
		return entries_ |
			   std::views::transform(
				   [this](const auto& entry)
				   {
					   return entry.second |
							  std::views::transform([this, shapeClass = entry.first](const auto& shape) { return resourcePath(shapeClass, shape); });
				   }) |
			   std::views::join;
	}

	auto iterClassShapePaths(std::string shapeClass) const
	{
		const auto iter = entries_.find(shapeClass);

		std::vector<std::string> shapes;
		if (iter != entries_.end())
		{
			shapes = iter->second;
		}

		return std::views::transform(
			std::move(shapes), [this, shapeClass = std::move(shapeClass)](const auto& entry) { return resourcePath(shapeClass, entry); });
	}

	[[nodiscard]] const std::unordered_map<std::string, std::vector<std::string>>& entries() const
	{
		return this->entries_;
	}

	[[nodiscard]] std::vector<ShapeRef> shapes() const
	{
		std::vector<ShapeRef> out;
		out.reserve(numShapes());
		for (const auto& [className, fileNames] : entries_)
		{
			for (const auto& fileName : fileNames)
			{
				out.push_back(ShapeRef{.className = className, .fileName = fileName});
			}
		}
		std::ranges::sort(out, {}, [](const ShapeRef& s) { return std::tie(s.className, s.fileName); });
		return out;
	}

	[[nodiscard]] std::vector<ShapeRef> randomClassBalancedShapes(const size_t limit, const std::uint32_t seed = std::random_device{}()) const
	{
		if (limit == 0 || limit >= numShapes())
		{
			return shapes();
		}

		std::mt19937 rng(seed);

		std::vector<std::vector<ShapeRef>> perClass;
		for (const auto& [className, fileNames] : entries_)
		{
			auto& classShapes = perClass.emplace_back();
			for (const auto& fileName : fileNames)
			{
				classShapes.push_back(ShapeRef{.className = className, .fileName = fileName});
			}
			std::ranges::shuffle(classShapes, rng);
		}
		std::ranges::shuffle(perClass, rng);

		std::vector<ShapeRef> out;
		out.reserve(limit);
		for (size_t round = 0; out.size() < limit; ++round)
		{
			bool tookAny = false;
			for (auto& classShapes : perClass)
			{
				if (round < classShapes.size() && out.size() < limit)
				{
					out.push_back(std::move(classShapes[round]));
					tookAny = true;
				}
			}
			if (!tookAny)
			{
				break;
			}
		}

		std::ranges::sort(out, {}, [](const ShapeRef& s) { return std::tie(s.className, s.fileName); });
		return out;
	}

	size_t numShapes() const
	{
		return std::ranges::fold_left(
			entries_ | std::views::values | std::views::transform([](const auto& shapes) { return shapes.size(); }), std::size_t{0}, std::plus{});
	}

	size_t numClasses() const
	{
		return this->entries_.size();
	}

	void exportStats() const
	{
		struct Row
		{
			std::string file, cls;
			size_t verts, faces, facetype;
			float bb_radius;
		};

		std::vector<std::pair<std::filesystem::path, std::string>> work;
		for (const auto& [className, meshNames] : entries_)
		{
			for (const auto& meshName : meshNames)
			{
				auto meshPath = resourcePath(className, meshName);
				work.emplace_back(meshPath, className);
			}
		}

		std::vector<std::optional<Row>> results(work.size());
		auto indices = std::views::iota(size_t{0}, work.size());

		std::for_each(
			std::execution::par, indices.begin(), indices.end(),
			[&](const size_t i)
			{
				const auto& [path, className] = work[i];
				std::optional<Mesh> mesh;

				if (path.extension() == ".obj")
				{
					if (auto loaded = Mesh::fromObj(path))
					{
						mesh.emplace(std::move(*loaded));
					}
				}
				else
				{
					std::cerr << "Unsupported mesh format: " << path.extension() << '\n';
					return;
				}

				if (!mesh)
				{
					std::cerr << "Failed to load mesh: " << path << '\n';
					return;
				}

				results[i] =
					Row{path.string(),			className, mesh->numVertices(), mesh->numFaces(), static_cast<std::uint8_t>(mesh->faceType()),
						mesh->bounds().radius()};
			});

		std::ofstream outputFile("shape_db_statistics.csv");
		outputFile << "file,class,vertices,faces,facetype,bounding_box\n";
		for (auto& row : results)
		{
			if (!row)
			{
				return;
			}
			outputFile << std::format("{},{},{},{},{},{}", row->file, row->cls, row->verts, row->faces, row->facetype, row->bb_radius) << '\n';
		}
		outputFile.close();
	}

	[[nodiscard]] std::filesystem::path resourcePath(const std::string& shapeClass = "", const std::string& filename = "") const
	{
		return this->shape_database_path / shapeClass / filename;
	}
};

#endif // MR_SHAPE_DATA_ACCESS_H