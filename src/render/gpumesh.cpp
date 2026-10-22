#include "gpumesh.h"

#include "../geometry/mesh.hpp"
#include <vector>


class Mesh;

GpuMesh::GpuMesh(const Mesh& mesh)
{
	axisLength_ = std::max(mesh.bounds().radius(), 1e-3F) * 1.25F;

	triangleMesh_.allocate(&GlObject::index_, mesh.triangleIndices());
	triangleMesh_.allocate(&GlObject::vertex_, mesh.positions());

	edgeMesh_.allocate(&GlObject::index_, mesh.edgeIndices());
	edgeMesh_.allocate(&GlObject::vertex_, triangleMesh_);

	auto [bbIndices, bbVertices] = generateBoundingBox(mesh);
	boundingBox_.allocate(&GlObject::index_, bbIndices);
	boundingBox_.allocate(&GlObject::vertex_, bbVertices);

	auto [gridIndices, gridVertices] = generateGrid();
	grid_.allocate(&GlObject::index_, gridIndices);
	grid_.allocate(&GlObject::vertex_, gridVertices);

	const auto hullIndices = generateHullEdges(mesh);
	hull_.allocate(&GlObject::index_, hullIndices);
	hull_.allocate(&GlObject::vertex_, triangleMesh_);

	const auto [normalIndices, normalVertices] = generateNormals(mesh, 0.005f);
	vertexNormals_.allocate(&GlObject::index_, normalIndices);
	vertexNormals_.allocate(&GlObject::vertex_, normalVertices);

	const auto [boundaryIndices, boundaryVertices] = generateBoundaryEdges(mesh);
	boundaryEdges_.allocate(&GlObject::index_, boundaryIndices);
	boundaryEdges_.allocate(&GlObject::vertex_, boundaryVertices);

	const auto axisVertices = generateAxis();
	axis_.allocate(&GlObject::vertex_, axisVertices);
}

GpuMesh::~GpuMesh() = default;

float GpuMesh::axisLength() const
{
	return axisLength_;
}

std::pair<std::vector<uint32_t>, std::vector<float>> GpuMesh::generateGrid()
{
	constexpr size_t halfCells = 100;
	constexpr float cellSize = .1;

	constexpr float extent = static_cast<float>(halfCells) * cellSize;

	std::vector<float> debugGridVertices;
	std::vector<std::uint32_t> debugGridIndices;

	debugGridVertices.reserve(((2 * halfCells) + 1) * 2 * 2 * 3);
	debugGridIndices.reserve(((2 * halfCells) + 1) * 2 * 2);

	for (auto i = -static_cast<int>(halfCells); i <= static_cast<int>(halfCells); ++i)
	{
		const float vPos = static_cast<float>(i) * cellSize;

		const auto baseA = static_cast<std::uint32_t>(debugGridVertices.size() / 3);
		debugGridVertices.insert(debugGridVertices.end(), {-extent, 0.0f, vPos, extent, 0.0f, vPos});
		debugGridIndices.push_back(baseA + 0);
		debugGridIndices.push_back(baseA + 1);

		const auto baseB = static_cast<std::uint32_t>(debugGridVertices.size() / 3);
		debugGridVertices.insert(debugGridVertices.end(), {vPos, 0.0f, -extent, vPos, 0.0f, extent});
		debugGridIndices.push_back(baseB + 0);
		debugGridIndices.push_back(baseB + 1);
	}

	return std::make_pair(debugGridIndices, debugGridVertices);
}

std::pair<std::vector<uint32_t>, std::vector<float>> GpuMesh::generateNormals(const Mesh& mesh, const float length)
{
	const auto positions = mesh.positions();
	const auto normals = mesh.normals();
	const std::size_t vertexCount = positions.size();

	std::vector<float> vertices;
	std::vector<std::uint32_t> indices;
	vertices.reserve(vertexCount * 2 * 3);
	indices.reserve(vertexCount * 2);

	for (auto&& [pos, vertex] : std::views::zip(positions, normals) | std::views::enumerate)
	{
		auto&& [position, normal] = vertex;

		auto start = position;
		auto end = position + normal * length;
		vertices.insert(vertices.end(), {start.x, start.y, start.z, end.x, end.y, end.z});

		const auto base = static_cast<std::uint32_t>(pos * 2);
		indices.push_back(base);
		indices.push_back(base + 1);
	}

	return std::make_pair(indices, vertices);
}

std::pair<std::vector<uint32_t>, std::vector<float>> GpuMesh::generateBoundaryEdges(const Mesh& mesh)
{
	const auto edges = mesh.boundaryEdges();
	const auto positions = mesh.positions();

	std::vector<float> vertices;
	std::vector<std::uint32_t> indices;

	vertices.reserve(edges.size() * 6);
	indices.reserve(edges.size() * 2);

	for (auto&& [pos, edge] : edges | std::views::enumerate)
	{
		const auto [left, right] = edge;

		const auto start = positions[left];
		const auto end = positions[right];

		vertices.insert(vertices.end(), {start.x, start.y, start.z, end.x, end.y, end.z});

		const auto base = static_cast<std::uint32_t>(pos * 2);
		indices.push_back(base);
		indices.push_back(base + 1);
	}
	return std::make_pair(indices, vertices);
}

std::pair<std::vector<uint32_t>, std::vector<float>> GpuMesh::generateBoundingBox(const Mesh& mesh)
{
	const auto& [min, max] = mesh.bounds();
	const std::vector boundingBoxVertices = {min.x, min.y, min.z, max.x, min.y, min.z, max.x, max.y, min.z, min.x, max.y, min.z,
											 min.x, min.y, max.z, max.x, min.y, max.z, max.x, max.y, max.z, min.x, max.y, max.z,};
	const std::vector<std::uint32_t> boundingBoxIndices = {0, 1, 1, 2, 2, 3, 3, 0, 4, 5, 5, 6, 6, 7, 7, 4, 0, 4, 1, 5, 2, 6, 3, 7};
	return std::make_pair(boundingBoxIndices, boundingBoxVertices);
}

std::vector<uint32_t> GpuMesh::generateHullEdges(const Mesh& mesh)
{
	const auto hull = mesh.convexHull();
	if (!hull.has_value())
	{
		return std::vector<uint32_t>();
	}

	const auto faces = hull->faceList();

	std::vector<std::uint32_t> indices;
	for (const auto face : faces.all())
	{
		for (size_t i = 0; i < face.size(); ++i)
		{
			indices.push_back(static_cast<std::uint32_t>(face[i]));
			indices.push_back(static_cast<std::uint32_t>(face[(i + 1) % face.size()]));
		}
	}

	return indices;
}

std::vector<float> GpuMesh::generateAxis()
{
	return {
		-1.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F,
		0.0F, -1.0F, 0.0F, 0.0F, 1.0F, 0.0F,
		0.0F, 0.0F, -1.0F, 0.0F, 0.0F, 1.0F,
	};
}
