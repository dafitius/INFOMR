#include "gpumesh.h"

#include "../io/mesh.hpp"

#include <cstdint>
#include <vector>


class Mesh;

namespace
{
	template <typename T>
	void allocate(QOpenGLBuffer& buffer, const std::span<T> data)
	{
		buffer.create();
		buffer.bind();
		buffer.setUsagePattern(QOpenGLBuffer::StaticDraw);
		buffer.allocate(data.data(), static_cast<int>(data.size() * sizeof(T)));
	}

	template <typename T>
	void allocate(QOpenGLBuffer& buffer, const std::vector<T>& data)
	{
		allocate(buffer, std::span{data});
	}
} // namespace

GpuMesh::GpuMesh(const Mesh& mesh)
	: triangleIndexCount_(static_cast<GLsizei>(mesh.triangle_indices().size()))
	, edgeIndexCount_(static_cast<GLsizei>(mesh.edge_indices().size()))
{
	initializeOpenGLFunctions();
	const auto& [min, max] = mesh.bounds();
	const std::vector boundingBoxVertices = {min.x, min.y, min.z, max.x, min.y, min.z, max.x, max.y, min.z, min.x, max.y, min.z,
											 min.x, min.y, max.z, max.x, min.y, max.z, max.x, max.y, max.z, min.x, max.y, max.z};
	const std::vector<std::uint32_t> boundingBoxIndices = {0, 1, 1, 2, 2, 3, 3, 0, 4, 5, 5, 6, 6, 7, 7, 4, 0, 4, 1, 5, 2, 6, 3, 7};

	allocate(vertexBuffer_, mesh.positions());
	allocate(triangleIndexBuffer_, mesh.triangle_indices());
	allocate(edgeIndexBuffer_, mesh.edge_indices());
	allocate(boundingBoxVertexBuffer_, boundingBoxVertices);
	allocate(boundingBoxIndexBuffer_, boundingBoxIndices);
	allocateGrid();
	allocateNormals(mesh, 0.005f);
	allocateBoundaryEdges(mesh);

	configureVertexArray(triangleVao_, vertexBuffer_, triangleIndexBuffer_);
	configureVertexArray(edgeVao_, vertexBuffer_, edgeIndexBuffer_);
	configureVertexArray(boundingBoxVao_, boundingBoxVertexBuffer_, boundingBoxIndexBuffer_);
	configureVertexArray(debugGridVao_, debugGridVertexBuffer_, debugGridIndexBuffer_);
	configureVertexArray(debugNormalsVao_, debugNormalsVertexBuffer_, debugNormalsIndexBuffer_);
	configureVertexArray(boundaryEdgesVao_, boundaryEdgesVertexBuffer_, boundaryEdgesIndexBuffer_);
}

GpuMesh::~GpuMesh()
{
	triangleVao_.destroy();
	edgeVao_.destroy();
	boundingBoxVao_.destroy();
	vertexBuffer_.destroy();
	triangleIndexBuffer_.destroy();
	edgeIndexBuffer_.destroy();
	boundingBoxVertexBuffer_.destroy();
	boundingBoxIndexBuffer_.destroy();
}

void GpuMesh::configureVertexArray(QOpenGLVertexArrayObject& vao, QOpenGLBuffer& vertexBuffer, QOpenGLBuffer& indexBuffer)
{
	vao.create();
	vao.bind();

	vertexBuffer.bind();
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);

	indexBuffer.bind();

	vao.release();
	vertexBuffer.release();
	indexBuffer.release();
}

void GpuMesh::drawTriangles()
{
	triangleVao_.bind();
	glDrawElements(GL_TRIANGLES, triangleIndexCount_, GL_UNSIGNED_INT, nullptr);
	triangleVao_.release();
}

void GpuMesh::drawEdges()
{
	edgeVao_.bind();
	glDrawElements(GL_LINES, edgeIndexCount_, GL_UNSIGNED_INT, nullptr);
	edgeVao_.release();
}

void GpuMesh::drawBoundingBox()
{
	boundingBoxVao_.bind();
	glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, nullptr);
	boundingBoxVao_.release();
}

void GpuMesh::drawGrid()
{
	debugGridVao_.bind();
	glDrawElements(GL_LINES, debugGridIndexCount_, GL_UNSIGNED_INT, nullptr);
	debugGridVao_.release();
}

void GpuMesh::drawNormals()
{
	debugNormalsVao_.bind();
	glDrawElements(GL_LINES, debugNormalsIndexCount_, GL_UNSIGNED_INT, nullptr);
	debugNormalsVao_.release();
}

void GpuMesh::drawBoundaryEdges()
{
	boundaryEdgesVao_.bind();
	glDrawElements(GL_LINES, boundaryEdgesIndexCount_, GL_UNSIGNED_INT, nullptr);
	boundaryEdgesVao_.release();
}

void GpuMesh::allocateGrid()
{
	constexpr int halfCells = 100;
	constexpr float y = 0.0f;
	constexpr float cellSize = .1;

	constexpr float extent = static_cast<float>(halfCells) * cellSize;

	std::vector<float> debugGridVertices;
	std::vector<std::uint32_t> debugGridIndices;

	debugGridVertices.reserve((2 * halfCells + 1) * 2 * 2 * 3);
	debugGridIndices.reserve((2 * halfCells + 1) * 2 * 2);

	for (int i = -halfCells; i <= halfCells; ++i)
	{
		const float v = static_cast<float>(i) * cellSize;

		const auto baseA = static_cast<std::uint32_t>(debugGridVertices.size() / 3);
		debugGridVertices.insert(debugGridVertices.end(), {
			-extent, y, v,
			 extent, y, v
		});
		debugGridIndices.push_back(baseA + 0);
		debugGridIndices.push_back(baseA + 1);

		const auto baseB = static_cast<std::uint32_t>(debugGridVertices.size() / 3);
		debugGridVertices.insert(debugGridVertices.end(), {
			v, y, -extent,
			v, y,  extent
		});
		debugGridIndices.push_back(baseB + 0);
		debugGridIndices.push_back(baseB + 1);
	}

	debugGridIndexCount_ = static_cast<GLsizei>(debugGridIndices.size());
	allocate(debugGridVertexBuffer_, debugGridVertices);
	allocate(debugGridIndexBuffer_, debugGridIndices);
}

void GpuMesh::allocateNormals(const Mesh& mesh, const float length)
{
	const auto positions = mesh.positions();
	const auto normals = mesh.normals();
	const std::size_t vertexCount = positions.size();

	std::vector<float> vertices;
	std::vector<std::uint32_t> indices;
	vertices.reserve(vertexCount * 2 * 3);
	indices.reserve(vertexCount * 2);

	for (auto&& [i, vertex] : std::views::zip(positions, normals) | std::views::enumerate) {
		auto&& [position, normal] = vertex;

		auto start = position;
		auto end = position + normal * length;
		vertices.insert(vertices.end(), {
			start.x, start.y, start.z,
			end.x, end.y, end.z
		});

		const auto base = static_cast<std::uint32_t>(i * 2);
		indices.push_back(base);
		indices.push_back(base + 1);
	}

	debugNormalsIndexCount_ = static_cast<GLsizei>(indices.size());
	allocate(debugNormalsVertexBuffer_, vertices);
	allocate(debugNormalsIndexBuffer_, indices);
}

void GpuMesh::allocateBoundaryEdges(const Mesh& mesh)
{
	const auto edges = mesh.boundary_edges();
	const auto positions = mesh.positions();

	std::vector<float> vertices;
	std::vector<std::uint32_t> indices;

	vertices.reserve(edges.size() * 6);
	indices.reserve(edges.size() * 2);

	for (auto&& [i, edge] : edges | std::views::enumerate)
	{
		const auto [a, b] = edge;

		const auto start = positions[a];
		const auto end = positions[b];

		vertices.insert(vertices.end(), {
			start.x, start.y, start.z,
			end.x, end.y, end.z
		});

		const auto base = static_cast<std::uint32_t>(i * 2);
		indices.push_back(base);
		indices.push_back(base + 1);
	}

	boundaryEdgesIndexCount_ = static_cast<GLsizei>(indices.size());

	allocate(boundaryEdgesVertexBuffer_, vertices);
	allocate(boundaryEdgesIndexBuffer_, indices);
}
