//
// Created by jaelk on 25-09-2026.
//

#ifndef QTTEST_OBJ_READER_H
#define QTTEST_OBJ_READER_H
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <vector>

#include "mesh.hpp"
#include "util.hpp"

class ObjFile
{
private:
	static std::optional<size_t> parse_index(std::string_view value, size_t element_count)
	{
		if (value.empty())
			return std::nullopt;

		try
		{
			const auto index = std::stoll(std::string(value));
			if (index == 0)
				return std::nullopt;

			if (index > 0)
			{
				const auto zero_based = index - 1;
				if (static_cast<size_t>(zero_based) >= element_count)
					return std::nullopt;

				return static_cast<size_t>(zero_based);
			}

			const auto relative = static_cast<size_t>(-index);
			if (relative > element_count)
				return std::nullopt;

			return element_count - relative;
		}
		catch (...)
		{
			return std::nullopt;
		}
	}

	static std::optional<size_t> parse_face_vertex(std::string_view value, size_t vertex_count)
	{
		const auto slash = value.find('/');
		if (slash != std::string_view::npos)
			value = value.substr(0, slash);

		return parse_index(value, vertex_count);
	}

	static bool parse_vertex(const std::string& line, std::array<float, 3>& vertex)
	{
		std::istringstream stream(line);
		char type;
		float x, y, z;

		if (!(stream >> type >> x >> y >> z))
			return false;

		if (type != 'v')
			return false;

		vertex = {x, y, z};
		return true;
	}

	static bool parse_normal(const std::string& line, std::array<float, 3>& normal)
	{
		std::istringstream stream(line);
		std::string type;
		float x, y, z;

		if (!(stream >> type >> x >> y >> z))
			return false;

		if (type != "vn")
			return false;

		normal = {x, y, z};
		return true;
	}

	static bool parse_face(const std::string& line, std::vector<size_t>& face, size_t vertex_count)
	{
		std::istringstream stream(line);
		std::string type;
		if (!(stream >> type) || type != "f")
			return false;

		std::string vertex;
		while (stream >> vertex)
		{
			const auto index = parse_face_vertex(vertex, vertex_count);
			if (!index)
				return false;
			face.push_back(*index);
		}
		// A face must have at least 3 vertices.
		return face.size() >= 3;
	}

public:
	static std::optional<Mesh> read(const std::filesystem::path& file_path)
	{
		auto self = Mesh();

		std::ifstream file(file_path);
		if (!file.is_open())
		{
			std::cerr << "Unable to open file!" << file_path.string() << std::endl;
			return std::nullopt;
		}

		std::vector<Mesh::Position> positions;
		std::vector<Mesh::Normal> normals;
		std::vector<Mesh::Face> faces;

		std::string line;
		size_t line_number = 0;

		while (std::getline(file, line))
		{
			++line_number;

			const auto first = line.find_first_not_of(" \t");
			if (first == std::string::npos || line[first] == '#')
				continue;

			std::istringstream stream(line.substr(first));
			std::string type;

			if (!(stream >> type))
				continue;

			if (type == "v")
			{
				std::array<float, 3> vertex;
				if (!parse_vertex(line.substr(first), vertex))
				{
					std::cerr << "Invalid vertex at line " << line_number << ": " << line << '\n';
					return std::nullopt;
				}
				positions.push_back(vertex);
			}
			else if (type == "vn")
			{
				std::array<float, 3> normal;
				if (!parse_normal(line.substr(first), normal))
				{
					std::cerr << "Invalid normal at line " << line_number << ": " << line << '\n';
					return std::nullopt;
				}
				normals.push_back(normal);
			}
			else if (type == "f")
			{
				std::vector<size_t> face;
				if (!parse_face(line.substr(first), face, positions.size()))
				{
					std::cerr << "Invalid face at line " << line_number << ": " << line << '\n';
					return std::nullopt;
				}
				faces.push_back(std::move(face));
			}
		}

		file.close();
		return Mesh(std::move(positions), std::move(normals), std::move(faces));
	}
};

#endif // QTTEST_OBJ_READER_H