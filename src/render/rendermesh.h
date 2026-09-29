#pragma once

#include <QVector3D>

#include <array>
#include <cstdint>
#include <span>
#include <vector>

struct Bounds
{
	QVector3D min;
	QVector3D max;

	[[nodiscard]] QVector3D center() const;
	[[nodiscard]] float radius() const;
};

struct RenderMesh
{
	std::vector<float> positions;
	std::vector<std::uint32_t> triangleIndices;
	std::vector<std::uint32_t> edgeIndices;
	Bounds bounds;
	size_t skippedFaces = 0;

	static RenderMesh fromPolygons(std::span<const std::array<float, 3>> vertices, std::span<const std::vector<size_t>> faces);
};
