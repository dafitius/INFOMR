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

		// Read the header variables
		auto [num_vertices, num_faces, num_edges] = [&file]
		{
			std::string line;
			std::getline(file, line);
			return *io_utils::parse_triplet<int>(line);
		}();

		std::vector<glm::vec3> positions(num_vertices);
		std::ranges::generate(
			positions,
			[&file]
			{
				std::string line;
				std::getline(file, line);
				const auto triplet = *io_utils::parse_triplet<float>(line);
				return glm::vec3{triplet[0], triplet[1], triplet[2]};
			});

		// read the index groups: "<count> i0 i1 ... "
		FaceList faces;
		faces.reserve(num_faces, static_cast<size_t>(num_faces) * 3);
		std::vector<FaceList::Index> face;
		for (int i = 0; i < num_faces; ++i)
		{
			std::string line;
			std::getline(file, line);

			face.clear();
			bool first = true;
			io_utils::for_each_token(
				line,
				[&](const std::string_view tok)
				{
					if (first)
					{
						first = false;
						return;
					}
					if (const auto idx = io_utils::parse_num<FaceList::Index>(tok))
						face.push_back(*idx);
				});

			if (face.size() >= 3)
				faces.push_back(face);
		}

		file.close();
		return Mesh(std::move(positions), {}, std::move(faces));
	}
};

#endif // QTTEST_OFF_READER_H