//
// Created by dafitius on 17-09-2026.
//

#ifndef QTTEST_OFF_READER_H
#define QTTEST_OFF_READER_H
#include <algorithm>
#include <array>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <ranges>
#include <set>
#include <span>
#include <tuple>
#include <vector>

#include "mesh.hpp"
#include "util.hpp"

class OffFile
{
public:
	static std::optional<Mesh> read(const std::filesystem::path& file_path)
	{
		std::ifstream file(file_path);
		if (!file.is_open())
		{
			std::cerr << "Unable to open file!" << file_path.string() << std::endl;
			return std::nullopt;
		}

		// Validate the OFF magic
		if (std::string line; std::getline(file, line) && line != "OFF")
		{
			std::cerr << "Unable to read file (magic is not OFF)!" << std::endl;
			return std::nullopt;
		}

		std::vector<Mesh::Position> positions;
		std::vector<Mesh::Face> faces;

		// Read the header variables
		auto [num_vertices, num_faces, num_edges] = [&file]
		{
			std::string line;
			std::getline(file, line);
			return *io_utils::parse_triplet<int>(line);
		}();

		positions = std::vector<std::array<float, 3>>(num_vertices);
		std::ranges::generate(
			positions,
			[&file]
			{
				std::string line;
				std::getline(file, line);
				return *io_utils::parse_triplet<float>(line);
			});

		// read the index groups
		faces = std::vector<std::vector<size_t>>(num_faces);
		std::ranges::generate(
			faces,
			[&file]
			{
				std::string line;
				std::getline(file, line);
				Mesh::Face face;
				face.reserve(4);

				io_utils::for_each_token(
					line.substr(1),
					[&](const std::string_view tok)
					{
						const auto idx = io_utils::parse_num<int>(tok);
						face.push_back(*idx);
					});

				return face;
			});

		file.close();
		return Mesh(std::move(positions), {}, std::move(faces));
	}
};

#endif // QTTEST_OFF_READER_H