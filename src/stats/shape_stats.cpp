//
// Created by DanioHans on 12-10-2026.
//

#include "shape_stats.hpp"

#include "../geometry/face_list.hpp"
#include "../geometry/mesh.hpp"
#include "../io/shape_db.hpp"
#include "../io/util.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <execution>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <glm/ext/vector_float3.hpp>
#include <glm/geometric.hpp>

namespace stats
{
	namespace
	{
		constexpr std::string_view csvHeader =
			"class,file,vertices,faces,face_type,"
			"min_x,min_y,min_z,max_x,max_y,max_z,"
			"bary_x,bary_y,bary_z,max_extent,"
			"boundary_edges,degenerate_faces,skipped_faces";
		constexpr size_t numCsvColumns = 18;

		/// @brief Counts faces with (near) zero area, e.g. collinear vertices or a vertex used twice
		size_t countDegenerateFaces(const Mesh& mesh)
		{
			const auto positions = mesh.positions();
			const auto diagonal = 2.0F * mesh.bounds().radius();
			const auto threshold = 1e-10F * diagonal * diagonal;

			return static_cast<size_t>(std::ranges::count_if(
				mesh.faces().all(),
				[&](const Mesh::Face face)
				{
					if (!FaceList::isValid(face, positions.size()))
					{
						return false;
					}

					// Sum of the fan triangles cross products, its length is twice the polygon area (for planar polygons)
					glm::vec3 areaVector{0.0F};
					for (size_t i = 1; i + 1 < face.size(); ++i)
					{
						areaVector += glm::cross(positions[face[i]] - positions[face[0]], positions[face[i + 1]] - positions[face[0]]);
					}
					return 0.5F * glm::length(areaVector) <= threshold;
				}));
		}

		std::optional<ShapeStats> parseRow(const std::string_view line)
		{
			std::vector<std::string_view> fields;
			std::ranges::copy(io_utils::split(line, ','), std::back_inserter(fields));
			if (fields.size() != numCsvColumns)
			{
				return std::nullopt;
			}

			bool valid = true;
			auto count = [&](const size_t column)
			{
				const auto value = io_utils::parseNum<size_t>(fields[column]);
				valid = valid && value.has_value();
				return value.value_or(0);
			};
			auto real = [&](const size_t column)
			{
				const auto value = io_utils::parseNum<float>(fields[column]);
				valid = valid && value.has_value();
				return value.value_or(0.0F);
			};
			auto vec = [&](const size_t firstColumn) { return glm::vec3{real(firstColumn), real(firstColumn + 1), real(firstColumn + 2)}; };

			const auto faceType = parseFaceType(fields[4]);

			// Column 14 (max_extent) is derived from the bounds, so it is not read back
			ShapeStats stats{
				.className = std::string(fields[0]),
				.fileName = std::string(fields[1]),
				.numVertices = count(2),
				.numFaces = count(3),
				.faceType = faceType.value_or(Mesh::FaceType::Mixed),
				.bounds = {.min = vec(5), .max = vec(8)},
				.baryCenter = vec(11),
				.numBoundaryEdges = count(15),
				.numDegenerateFaces = count(16),
				.numSkippedFaces = count(17),
			};

			if (!valid || !faceType)
			{
				return std::nullopt;
			}
			return stats;
		}
	} // namespace

	ShapeStats compute(const Mesh& mesh, std::string className, std::string fileName)
	{
		return ShapeStats{
			.className = std::move(className),
			.fileName = std::move(fileName),
			.numVertices = mesh.numVertices(),
			.numFaces = mesh.numFaces(),
			.faceType = mesh.faceType(),
			.bounds = mesh.bounds(),
			.baryCenter = mesh.baryCenter(),
			.numBoundaryEdges = mesh.boundaryEdges().size(),
			.numDegenerateFaces = countDegenerateFaces(mesh),
			.numSkippedFaces = mesh.numSkippedFaces(),
		};
	}

	DatabaseStats compute(const ShapeDb& database)
	{
		struct Job
		{
			std::string className;
			std::string fileName;
			std::optional<ShapeStats> result;
		};

		std::vector<Job> jobs;
		jobs.reserve(database.numShapes());
		for (const auto& [className, fileNames] : database.entries())
		{
			for (const auto& fileName : fileNames)
			{
				jobs.push_back(Job{.className = className, .fileName = fileName, .result = std::nullopt});
			}
		}

		std::for_each(
			std::execution::par, jobs.begin(), jobs.end(),
			[&database](Job& job)
			{
				const auto path = database.resourcePath(job.className, job.fileName);
				if (path.extension() != ".obj")
				{
					return;
				}
				if (const auto mesh = Mesh::fromObj(path))
				{
					job.result = compute(*mesh, job.className, job.fileName);
				}
			});

		DatabaseStats stats;
		stats.shapes.reserve(jobs.size());
		for (auto& job : jobs)
		{
			if (job.result)
			{
				stats.shapes.push_back(std::move(*job.result));
			}
			else
			{
				stats.failed.push_back(database.resourcePath(job.className, job.fileName));
			}
		}

		std::ranges::sort(stats.shapes, {}, [](const ShapeStats& s) { return std::tie(s.className, s.fileName); });
		return stats;
	}

	std::span<const Column> columns()
	{
		static constexpr std::array all{
			Column{"vertices", [](const ShapeStats& s) { return static_cast<double>(s.numVertices); }},
			Column{"faces", [](const ShapeStats& s) { return static_cast<double>(s.numFaces); }},
			Column{"extent_x", [](const ShapeStats& s) { return static_cast<double>(s.extent().x); }},
			Column{"extent_y", [](const ShapeStats& s) { return static_cast<double>(s.extent().y); }},
			Column{"extent_z", [](const ShapeStats& s) { return static_cast<double>(s.extent().z); }},
			Column{"max_extent", [](const ShapeStats& s) { return static_cast<double>(s.maxExtent()); }},
			Column{"bary_x", [](const ShapeStats& s) { return static_cast<double>(s.baryCenter.x); }},
			Column{"bary_y", [](const ShapeStats& s) { return static_cast<double>(s.baryCenter.y); }},
			Column{"bary_z", [](const ShapeStats& s) { return static_cast<double>(s.baryCenter.z); }},
			Column{"bary_distance", [](const ShapeStats& s) { return static_cast<double>(glm::length(s.baryCenter)); }},
			Column{"boundary_edges", [](const ShapeStats& s) { return static_cast<double>(s.numBoundaryEdges); }},
			Column{"degenerate_faces", [](const ShapeStats& s) { return static_cast<double>(s.numDegenerateFaces); }},
			Column{"skipped_faces", [](const ShapeStats& s) { return static_cast<double>(s.numSkippedFaces); }},
		};
		return all;
	}

	std::optional<Column> findColumn(const std::string_view name)
	{
		const auto all = columns();
		const auto found = std::ranges::find(all, name, &Column::name);
		return found != all.end() ? std::make_optional(*found) : std::nullopt;
	}

	std::vector<double> columnValues(const std::span<const ShapeStats> shapes, const Column& column)
	{
		std::vector<double> values;
		values.reserve(shapes.size());
		std::ranges::transform(shapes, std::back_inserter(values), column.value);
		return values;
	}

	std::string_view toString(const Mesh::FaceType faceType)
	{
		switch (faceType)
		{
			case Mesh::FaceType::Triangles: return "triangles";
			case Mesh::FaceType::Quads: return "quads";
			case Mesh::FaceType::Mixed: return "mixed";
		}
		std::unreachable();
	}

	std::optional<Mesh::FaceType> parseFaceType(const std::string_view text)
	{
		for (const auto faceType : {Mesh::FaceType::Triangles, Mesh::FaceType::Quads, Mesh::FaceType::Mixed})
		{
			if (toString(faceType) == text)
			{
				return faceType;
			}
		}
		return std::nullopt;
	}

	bool writeCsv(const std::filesystem::path& path, const std::span<const ShapeStats> shapes)
	{
		std::ofstream file(path);
		if (!file.is_open())
		{
			std::cerr << "Unable to write file: " << path.string() << '\n';
			return false;
		}

		file << csvHeader << '\n';
		for (const auto& shape : shapes)
		{
			const auto& [min, max] = shape.bounds;
			const auto& bary = shape.baryCenter;
			file << std::format(
				"{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{}\n", shape.className, shape.fileName, shape.numVertices, shape.numFaces,
				toString(shape.faceType), min.x, min.y, min.z, max.x, max.y, max.z, bary.x, bary.y, bary.z, shape.maxExtent(), shape.numBoundaryEdges,
				shape.numDegenerateFaces, shape.numSkippedFaces);
		}
		return file.good();
	}

	std::optional<std::vector<ShapeStats>> readCsv(const std::filesystem::path& path)
	{
		std::ifstream file(path);
		if (!file.is_open())
		{
			std::cerr << "Unable to open file: " << path.string() << '\n';
			return std::nullopt;
		}

		std::string line;
		if (!std::getline(file, line) || io_utils::trim(line) != csvHeader)
		{
			std::cerr << "Unexpected statistics header in: " << path.string() << '\n';
			return std::nullopt;
		}

		std::vector<ShapeStats> shapes;
		for (size_t lineNumber = 2; std::getline(file, line); ++lineNumber)
		{
			const auto trimmed = io_utils::trim(line);
			if (trimmed.empty())
			{
				continue;
			}

			auto row = parseRow(trimmed);
			if (!row)
			{
				std::cerr << std::format("Malformed statistics row {} in: {}\n", lineNumber, path.string());
				return std::nullopt;
			}
			shapes.push_back(std::move(*row));
		}
		return shapes;
	}
} // namespace stats
