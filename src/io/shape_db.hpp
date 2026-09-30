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
#include <filesystem>
#include <fstream>
#include <iostream>
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

	size_t num_shapes() const
	{
		return std::ranges::fold_left(
			entries_ | std::views::values | std::views::transform([](const auto& shapes) { return shapes.size(); }), std::size_t{0}, std::plus{});
	}

	size_t num_classes() const
	{
		return this->entries_.size();
	}

	bool export_stats(size_t max_per_class)
	{
		std::ofstream outputFile;
		outputFile.open("shape_db_statistics.csv");
		outputFile << "file, class, vertices, faces, facetype, bounding_box" << std::endl;
		for (const auto& className : iter_class_names())
		{
			size_t count = 0;
			for (const auto& meshPath : iter_class_shape_paths(className))
			{
				if (count >= max_per_class && max_per_class > 0)
				{
					break;
				}
				count++;

				const std::filesystem::path path = meshPath;
				std::optional<Mesh> mesh;

				if (path.extension() == ".obj")
				{
					mesh = ObjFile::read(path);
				}
				else if (path.extension() == ".off")
				{
					mesh = OffFile::read(path);
				}
				else
				{
					std::cerr << "Unsupported mesh format: " << path.extension() << '\n';
					return false;
				}

				if (!mesh)
				{
					std::cerr << "Failed to load mesh: " << path << '\n';
					return false;
				}
				std::ostringstream os;
				os << path << ", " << className << ", " << mesh->num_vertices() << ", " << mesh->num_faces() << ", " << mesh->face_type()
				   << std::endl;
				std::string s = os.str();
				outputFile << s;
			}
		}
		outputFile.close();
		return true;
		// TODO: Create CSV file with stats of every mesh:
		// V the class of the shape
		// V the number of faces and vertices of the shape
		// V the type of faces (e.g. only triangles, only quads, mixes of triangles and quads)
		// - the axis-aligned 3D bounding box of the shapes
	}

private:
	[[nodiscard]] std::filesystem::path resource_path(const std::string& class_ = "", const std::string& filename = "") const
	{
		return this->shape_database_path / class_ / filename;
	}
};

#endif // MR_SHAPE_DATA_ACCESS_H