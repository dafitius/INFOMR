#include "rendermesh.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

QVector3D Bounds::center() const
{
	return (min + max) * 0.5F;
}

float Bounds::radius() const
{
	return (max - min).length() * 0.5F;
}

namespace
{
	Bounds computeBounds(std::span<const std::array<float, 3>> vertices)
	{
		constexpr float infinity = std::numeric_limits<float>::infinity();

		auto min = QVector3D(infinity, infinity, infinity);
		auto max = QVector3D(-infinity, -infinity, -infinity);

		for (const auto& [x, y, z] : vertices)
		{
			if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
			{
				continue;
			}

			min = QVector3D(std::min(min.x(), x), std::min(min.y(), y), std::min(min.z(), z));
			max = QVector3D(std::max(max.x(), x), std::max(max.y(), y), std::max(max.z(), z));
		}

		if (min.x() > max.x())
		{
			return {};
		}

		return {min, max};
	}
} // namespace

RenderMesh RenderMesh::fromPolygons(std::span<const std::array<float, 3>> vertices, std::span<const std::vector<size_t>> faces)
{
	auto mesh = RenderMesh();

	mesh.positions.reserve(vertices.size() * 3);
	for (const auto& [x, y, z] : vertices)
	{
		mesh.positions.insert(mesh.positions.end(), {x, y, z});
	}

	mesh.bounds = computeBounds(vertices);

	auto isValidFace = [vertexCount = vertices.size()](const std::vector<size_t>& face)
	{ return face.size() >= 3 && std::ranges::all_of(face, [vertexCount](const size_t index) { return index < vertexCount; }); };

	std::vector<std::pair<std::uint32_t, std::uint32_t>> edges;

	for (const auto& face : faces)
	{
		if (!isValidFace(face))
		{
			++mesh.skippedFaces;
			continue;
		}

		// Fan triangulation, only correct for convex polygons.
		for (size_t i = 1; i + 1 < face.size(); ++i)
		{
			mesh.triangleIndices.insert(
				mesh.triangleIndices.end(),
				{static_cast<std::uint32_t>(face[0]), static_cast<std::uint32_t>(face[i]), static_cast<std::uint32_t>(face[i + 1])});
		}

		for (size_t i = 0; i < face.size(); ++i)
		{
			const auto [lo, hi] = std::minmax({static_cast<std::uint32_t>(face[i]), static_cast<std::uint32_t>(face[(i + 1) % face.size()])});
			if (lo != hi)
			{
				edges.emplace_back(lo, hi);
			}
		}
	}

	std::ranges::sort(edges);
	const auto duplicates = std::ranges::unique(edges);
	edges.erase(duplicates.begin(), duplicates.end());

	mesh.edgeIndices.reserve(edges.size() * 2);
	for (const auto& [lo, hi] : edges)
	{
		mesh.edgeIndices.insert(mesh.edgeIndices.end(), {lo, hi});
	}

	return mesh;
}
