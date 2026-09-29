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

#include "util.hpp"

class OffFile
{
	std::vector<std::array<float, 3>> positions_;
	std::vector<std::vector<size_t>> faces_;

public:
	static std::optional<OffFile> read(const std::filesystem::path& file_path)
	{
		auto self = OffFile();

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

		// Read the header variables
		auto [num_vertices, num_faces, num_edges] = [&file]
		{
			std::string line;
			std::getline(file, line);
			const auto words = io_utils::split(line, " ");
			return std::make_tuple(std::stoi(words[0]), std::stoi(words[1]), std::stoi(words[2]));
		}();

		self.positions_ = std::vector<std::array<float, 3>>(num_vertices);
		std::ranges::generate(
			self.positions_,
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
		self.faces_ = std::vector<std::vector<size_t>>(num_faces);
		std::ranges::generate(
			self.faces_,
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
		return self;
	}

	[[nodiscard]] size_t num_vertices() const
	{
		return positions_.size();
	}

	[[nodiscard]] size_t num_faces() const
	{
		return faces_.size();
	}

	[[nodiscard]] size_t num_edges() const
	{
		auto edges = std::set<std::tuple<size_t, size_t>>(); // list of all edges in the mesh
		std::ranges::for_each(
			faces_,
			[&edges](auto face)
			{
				face.push_back(face[0]); // Copy last idx to the end to close the loop
				std::ranges::for_each(
					std::views::slide(face, 2),
					[&edges](auto v)
					{
						auto [hi, lo] = std::ranges::minmax(v[0], v[1]);
						if (lo != hi)
						{
							edges.insert({lo, hi});
						}
					});
			});
		return edges.size();
	}

	[[nodiscard]] std::span<const std::array<float, 3>> positions() const
	{
		return positions_;
	}

	[[nodiscard]] std::span<const std::vector<size_t>> faces() const
	{
		return faces_;
	}

	[[nodiscard]] std::vector<float> vertex_buffer() const
	{
		auto buffer = std::vector<float>(positions_.size() * 3);

		auto flattened = positions_ | std::views::join;
		std::ranges::copy(flattened, buffer.begin());

		return buffer;
	};

	[[nodiscard]] std::vector<size_t> index_buffer() const
	{
		assert(std::ranges::all_of(faces_, [](const auto& face) { return face.size() == 3; }) && "index_buffer() assumes all faces are triangles!!");

		auto buffer = std::vector<size_t>(faces_.size() * 3);

		auto flattened = faces_ | std::views::join;
		std::ranges::copy(flattened, buffer.begin());

		return buffer;
	}
};

#endif // QTTEST_OFF_READER_H