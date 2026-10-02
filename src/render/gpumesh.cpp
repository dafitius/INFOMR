#include "gpumesh.h"

#include "../io/mesh.hpp"

#include <cstdint>
#include <vector>


class Mesh;

namespace
{
	template <typename T>
	void allocate(QOpenGLBuffer& buffer, const std::vector<T>& data)
	{
		buffer.create();
		buffer.bind();
		buffer.setUsagePattern(QOpenGLBuffer::StaticDraw);
		buffer.allocate(data.data(), static_cast<int>(data.size() * sizeof(T)));
	}

	template <typename T>
	void allocate(QOpenGLBuffer& buffer, const std::span<T> data)
	{
		buffer.create();
		buffer.bind();
		buffer.setUsagePattern(QOpenGLBuffer::StaticDraw);
		buffer.allocate(data.data(), static_cast<int>(data.size() * sizeof(T)));
	}
} // namespace

GpuMesh::GpuMesh(const Mesh& mesh)
	: triangleIndexCount_(static_cast<GLsizei>(mesh.triangle_indices().size()))
	, edgeIndexCount_(static_cast<GLsizei>(mesh.edge_indices().size()))
	, boundingBoxIndexCount_(24)
{
	initializeOpenGLFunctions();
	const auto& bounds = mesh.bounds();
	const std::vector boundingBoxVertices = {bounds.min.x, bounds.min.y, bounds.min.z, bounds.max.x, bounds.min.y, bounds.min.z,
											 bounds.max.x, bounds.max.y, bounds.min.z, bounds.min.x, bounds.max.y, bounds.min.z,
											 bounds.min.x, bounds.min.y, bounds.max.z, bounds.max.x, bounds.min.y, bounds.max.z,
											 bounds.max.x, bounds.max.y, bounds.max.z, bounds.min.x, bounds.max.y, bounds.max.z};
	const std::vector<std::uint32_t> boundingBoxIndices = {0, 1, 1, 2, 2, 3, 3, 0, 4, 5, 5, 6, 6, 7, 7, 4, 0, 4, 1, 5, 2, 6, 3, 7};

	allocate(vertexBuffer_, mesh.positions());
	allocate(triangleIndexBuffer_, mesh.triangle_indices());
	allocate(edgeIndexBuffer_, mesh.edge_indices());
	allocate(boundingBoxVertexBuffer_, boundingBoxVertices);
	allocate(boundingBoxIndexBuffer_, boundingBoxIndices);

	configureVertexArray(triangleVao_, vertexBuffer_, triangleIndexBuffer_);
	configureVertexArray(edgeVao_, vertexBuffer_, edgeIndexBuffer_);
	configureVertexArray(boundingBoxVao_, boundingBoxVertexBuffer_, boundingBoxIndexBuffer_);
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
	glDrawElements(GL_LINES, boundingBoxIndexCount_, GL_UNSIGNED_INT, nullptr);
	boundingBoxVao_.release();
}
