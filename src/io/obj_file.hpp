//
// Created by jaelk on 25-09-2026.
//

#ifndef QTTEST_OBJ_READER2_H
#define QTTEST_OBJ_READER2_H
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <vector>

#include "mesh.hpp"
#include "util.hpp"

class ObjFile
{
public:
	static std::optional<Mesh> read(const std::filesystem::path& file_path)
	{
		// Read entire file into memory
		std::ifstream file(file_path, std::ios::binary);
		if (!file.is_open())
		{
			std::cerr << "Can't open file" << file_path.string() << std::endl;
			return std::nullopt;
		}

		file.seekg(0, std::ios::end);
		const auto total_size = file.tellg();
		file.seekg(0, std::ios::beg);

		std::string buffer(total_size, '\0');
		if (!file.read(buffer.data(), total_size))
		{
			std::cerr << "Failed to read file: " << file_path.string() << std::endl;
			return std::nullopt;
		}
		file.close();

		std::vector<Mesh::Position> positions = {};
		std::vector<Mesh::Normal> normals = {};
		std::vector<Mesh::Face> faces = {};

		auto lines = std::string_view(buffer) | std::views::split('\n') |
					 std::views::transform(
						 [](auto part)
						 {
							 const std::string_view sv{part.data(), static_cast<size_t>(part.size())};
							 return io_utils::trim(sv);
						 });

		for (auto line : lines)
		{
			if (line.size() < 2 || line[0] == '#')
				continue;

			const char tag0 = line[0];
			const char tag1 = line[1];

			if (tag0 == 'v' && tag1 == ' ')
			{
				auto payload = io_utils::trim(line.substr(1));
				if (auto triplet = io_utils::parse_triplet<float>(payload))
					positions.push_back(*triplet);
			}
			else if (tag0 == 'v' && tag1 == 'n')
			{
				auto payload = io_utils::trim(line.substr(2));
				if (auto triplet = io_utils::parse_triplet<float>(payload))
					normals.push_back(*triplet);
			}
			else if (tag0 == 'f' && tag1 == ' ')
			{
				Mesh::Face face;
				face.reserve(4);

				io_utils::for_each_token(
					line.substr(1),
					[&](const std::string_view tok)
					{
						const auto idx = parse_face_index(tok, positions.size());
						face.push_back(*idx);
					});

				if (face.size() < 3)
					return std::nullopt;
				faces.push_back(std::move(face));
			}
		}
		return Mesh(std::move(positions), std::move(normals), std::move(faces));
	}

	static std::optional<size_t> parse_face_index(std::string_view tok, const size_t count)
	{
		if (const auto slash = tok.find('/'); slash != std::string_view::npos)
			tok = tok.substr(0, slash);
		tok = io_utils::trim(tok);
		if (tok.empty())
			return std::nullopt;

		const auto idx = io_utils::parse_num<size_t>(tok);
		if (!idx || *idx == 0)
			return std::nullopt;

		const auto z = idx.value() - 1;
		if (z >= count)
			return std::nullopt;
		return z;
	}
};

#endif // QTTEST_OBJ_READER2_H