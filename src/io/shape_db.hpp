//
// Created by dafitius on 17-09-2026.
//

#ifndef MR_SHAPE_DATA_ACCESS_H
#define MR_SHAPE_DATA_ACCESS_H
#include "mesh.hpp"
#include "obj_file.hpp"
#include "off_file.hpp"
#include <QString>
#include <algorithm>
#include <execution>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <optional>
#include <ranges>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

class ShapeDb
{
	std::filesystem::path shape_database_path;
	std::unordered_map<std::string, std::vector<std::string>> entries_; // map of shape class as key and shape names as value

public:
	static std::optional<ShapeDb> open(const std::filesystem::path& shape_database_path)
	{
		auto self = ShapeDb();

		if (!std::filesystem::exists(shape_database_path))
		{
			std::cerr << "Unable to open file!" << shape_database_path.string() << std::endl;
			return std::nullopt;
		}

		auto entries = std::filesystem::directory_iterator(shape_database_path) |
					   std::views::filter([](const auto& item) { return item.is_directory(); }) |
					   std::views::transform(
						   [&](const auto& item)
						   {
							   auto folder_names = std::vector<std::string>();
							   auto view = std::filesystem::directory_iterator(item.path()) |
										   std::views::filter([](const auto& file) { return file.is_regular_file(); }) |
										   std::views::transform([&](const auto& file) { return file.path().filename().string(); });

							   std::ranges::copy(view, std::back_inserter(folder_names));
							   return std::pair<std::string, std::vector<std::string>>(item.path().filename().string(), std::move(folder_names));
						   });

		std::ranges::for_each(entries, [&self](const auto& entry) { self.entries_.emplace(entry.first, entry.second); });

		self.shape_database_path = shape_database_path;

		return self;
	}

	auto iter_class_names() const
	{
		return entries_ | std::views::keys;
	}

	auto iter_all_shape_paths() const
	{
		return entries_ |
			   std::views::transform(
				   [this](const auto& entry)
				   {
					   return entry.second |
							  std::views::transform([this, class_ = entry.first](const auto& shape) { return resource_path(class_, shape); });
				   }) |
			   std::views::join;
	}

	auto iter_class_shape_paths(std::string class_) const
	{
		const auto iter = entries_.find(class_);

		std::vector<std::string> shapes;
		if (iter != entries_.end())
			shapes = iter->second;

		return std::views::transform(
			std::move(shapes), [this, class_ = std::move(class_)](const auto& entry) { return resource_path(class_, entry); });
	}

	const std::unordered_map<std::string, std::vector<std::string>>& entries()
	{
		return this->entries_;
	}

	size_t num_shapes() const
	{
		return std::ranges::fold_left(
			entries_ | std::views::values | std::views::transform([](const auto& shapes) { return shapes.size(); }), std::size_t{0}, std::plus{});
	}

	size_t num_classes() const
	{
		return this->entries_.size();
	}

	void export_stats() const
	{
		struct Row
		{
			std::string file, cls;
			size_t verts, faces, facetype;
		};

		std::vector<std::pair<std::filesystem::path, std::string>> work;
		for (const auto& [className, meshNames] : entries_)
		{
			for (const auto& meshName : meshNames)
			{
				auto meshPath = resource_path(className, meshName);
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
					mesh = ObjFile::read(path);
				else if (path.extension() == ".off")
					mesh = OffFile::read(path);
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

				results[i] = Row{path.string(), className, mesh->num_vertices(), mesh->num_faces(), mesh->face_type()};
			});

		std::ofstream outputFile("shape_db_statistics.csv");
		outputFile << "file, class, vertices, faces, facetype, bounding_box\n";
		for (auto& row : results)
		{
			if (!row)
				return;
			outputFile << row->file << ", " << row->cls << ", " << row->verts << ", " << row->faces << ", " << row->facetype << '\n';
		}
		outputFile.close();
	}

private:
	[[nodiscard]] std::filesystem::path resource_path(const std::string& class_ = "", const std::string& filename = "") const
	{
		return this->shape_database_path / class_ / filename;
	}
};

#endif // MR_SHAPE_DATA_ACCESS_H