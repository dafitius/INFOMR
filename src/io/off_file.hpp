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
			const auto words = io_utils::split(line, " ");
			return std::make_tuple(std::stoi(words[0]), std::stoi(words[1]), std::stoi(words[2]));
		}();

		positions = std::vector<std::array<float, 3>>(num_vertices);
		std::ranges::generate(
			positions,
			[&file]
			{
				std::string line;
				std::getline(file, line);
				const auto coords = io_utils::split(line, " ");
				return std::array{
					io_utils::string_to_float<float>(coords[0]), io_utils::string_to_float<float>(coords[1]),
					io_utils::string_to_float<float>(coords[2])};
			});

		// read the index groups
		faces = std::vector<std::vector<size_t>>(num_faces);
		std::ranges::generate(
			faces,
			[&file]
			{
				std::string line;
				std::getline(file, line);
				const auto idx_str = io_utils::split(line, " ");
				std::vector<size_t> idx(std::stoi(idx_str[0]));
				std::ranges::transform(idx_str.begin() + 1, idx_str.end(), idx.begin(), [](auto& index) { return std::stoi(index); });
				return idx;
			});

		file.close();
		return Mesh(std::move(positions), {}, std::move(faces));
	}
};

#endif // QTTEST_OFF_READER_H