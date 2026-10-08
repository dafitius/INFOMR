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
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

#include "face_list.hpp"
#include "util.hpp"

class ObjFile
{
public:
	template <typename Init_>
	static bool read(const std::filesystem::path& file_path, Init_ init)
	{
		// Read entire file into memory
		std::ifstream file(file_path, std::ios::binary);
		if (!file.is_open())
		{
			std::cerr << "Can't open file" << file_path.string() << std::endl;
			return false;
		}

		file.seekg(0, std::ios::end);
		const auto total_size = file.tellg();
		file.seekg(0, std::ios::beg);

		std::string buffer(total_size, '\0');
		if (!file.read(buffer.data(), total_size))
		{
			std::cerr << "Failed to read file: " << file_path.string() << std::endl;
			return false;
		}
		file.close();

		const auto counts = count_elements(buffer);

		std::vector<std::array<float, 3>> positions;
		std::vector<std::array<float, 3>> normals;
		FaceList faces;
		positions.reserve(counts.positions);
		normals.reserve(counts.normals);
		faces.reserve(counts.faces, counts.faces * 3);

		std::vector<FaceList::Index> face; // scratch, reused for every face
		face.reserve(8);

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
				face.clear();
				bool valid = true;

				io_utils::for_each_token(
					line.substr(1),
					[&](const std::string_view tok)
					{
						const auto idx = parse_face_index(tok, positions.size());
						if (idx)
							face.push_back(static_cast<FaceList::Index>(*idx));
						else
							valid = false;
					});

				if (valid && face.size() >= 3)
					faces.push_back(face);
			}
		}
		init(std::move(positions), std::move(normals), std::move(faces));
		return true;
	}

private:
	struct ElementCounts
	{
		size_t positions = 0;
		size_t normals = 0;
		size_t faces = 0;
	};

	static ElementCounts count_elements(const std::string_view text)
	{
		ElementCounts counts;
		size_t pos = 0;
		while (pos + 1 < text.size())
		{
			const char c0 = text[pos];
			const char c1 = text[pos + 1];
			if (c0 == 'v' && c1 == ' ')
				++counts.positions;
			else if (c0 == 'v' && c1 == 'n')
				++counts.normals;
			else if (c0 == 'f' && c1 == ' ')
				++counts.faces;

			const auto newline = text.find('\n', pos);
			if (newline == std::string_view::npos)
				break;
			pos = newline + 1;
		}
		return counts;
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