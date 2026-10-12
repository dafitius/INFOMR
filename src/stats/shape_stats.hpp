//
// Created by DanioHans on 12-10-2026.
//

#ifndef MULTIMEDIARETRIEVAL_SHAPE_STATS_HPP
#define MULTIMEDIARETRIEVAL_SHAPE_STATS_HPP

#include "../geometry/mesh.hpp"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <glm/vec3.hpp>

class ShapeDb;

namespace stats
{
	/// @brief Properties of a single shape in the database, one row in the statistics CSV
	struct ShapeStats
	{
		std::string className;
		std::string fileName;
		size_t numVertices{};
		size_t numFaces{};
		Mesh::FaceType faceType{};
		Mesh::Bounds bounds{};
		glm::vec3 baryCenter{};
		size_t numBoundaryEdges{};
		size_t numDegenerateFaces{};
		size_t numSkippedFaces{};

		[[nodiscard]] glm::vec3 extent() const
		{
			return bounds.max - bounds.min;
		}

		[[nodiscard]] float maxExtent() const
		{
			const auto size = extent();
			return std::max({size.x, size.y, size.z});
		}
	};

	struct DatabaseStats
	{
		std::vector<ShapeStats> shapes;
		std::vector<std::filesystem::path> failed; // shapes that could not be loaded
	};

	/// @brief Named numeric property of a shape, used to summarize or plot one property over many shapes
	struct Column
	{
		std::string_view name;
		double (*value)(const ShapeStats&);
	};

	[[nodiscard]] ShapeStats compute(const Mesh& mesh, std::string className, std::string fileName);

	/// @brief Loads every shape in the database (in parallel) and computes its statistics
	[[nodiscard]] DatabaseStats compute(const ShapeDb& database);

	[[nodiscard]] std::span<const Column> columns();
	[[nodiscard]] std::optional<Column> findColumn(std::string_view name);
	[[nodiscard]] std::vector<double> columnValues(std::span<const ShapeStats> shapes, const Column& column);

	[[nodiscard]] std::string_view toString(Mesh::FaceType faceType);
	[[nodiscard]] std::optional<Mesh::FaceType> parseFaceType(std::string_view text);

	bool writeCsv(const std::filesystem::path& path, std::span<const ShapeStats> shapes);
	[[nodiscard]] std::optional<std::vector<ShapeStats>> readCsv(const std::filesystem::path& path);
} // namespace stats

#endif // MULTIMEDIARETRIEVAL_SHAPE_STATS_HPP
