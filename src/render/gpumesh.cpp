#include "gpumesh.h"

#include <cstdint>
#include <vector>

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
}

GpuMesh::GpuMesh(const RenderMesh& mesh)
	: triangleIndexCount_(static_cast<GLsizei>(mesh.triangleIndices.size()))
	, edgeIndexCount_(static_cast<GLsizei>(mesh.edgeIndices.size()))
{
	initializeOpenGLFunctions();

	allocate(vertexBuffer_, mesh.positions);
	allocate(triangleIndexBuffer_, mesh.triangleIndices);
	allocate(edgeIndexBuffer_, mesh.edgeIndices);

	configureVertexArray(triangleVao_, triangleIndexBuffer_);
	configureVertexArray(edgeVao_, edgeIndexBuffer_);
}

GpuMesh::~GpuMesh()
{
	triangleVao_.destroy();
	edgeVao_.destroy();
	vertexBuffer_.destroy();
	triangleIndexBuffer_.destroy();
	edgeIndexBuffer_.destroy();
}

void GpuMesh::configureVertexArray(QOpenGLVertexArrayObject& vao, QOpenGLBuffer& indexBuffer)
{
	vao.create();
	vao.bind();

	vertexBuffer_.bind();
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);

	indexBuffer.bind();

	vao.release();
	vertexBuffer_.release();
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
