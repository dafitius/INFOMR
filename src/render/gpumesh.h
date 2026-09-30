#pragma once

#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLVertexArrayObject>

#include "rendermesh.h"

// Owns the GPU buffers of a mesh. Must be created and destroyed while its OpenGL context is current.
class GpuMesh : protected QOpenGLFunctions
{
public:
	explicit GpuMesh(const RenderMesh& mesh);
	~GpuMesh();

	GpuMesh(const GpuMesh&) = delete;
	GpuMesh& operator=(const GpuMesh&) = delete;

	void drawTriangles();
	void drawEdges();
	void drawBoundingBox();

private:
	void configureVertexArray(QOpenGLVertexArrayObject& vao, QOpenGLBuffer& vertexBuffer, QOpenGLBuffer& indexBuffer);

	QOpenGLBuffer vertexBuffer_ = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
	QOpenGLBuffer triangleIndexBuffer_ = QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
	QOpenGLBuffer edgeIndexBuffer_ = QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
	QOpenGLBuffer boundingBoxVertexBuffer_ = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
	QOpenGLBuffer boundingBoxIndexBuffer_ = QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);

	QOpenGLVertexArrayObject triangleVao_;
	QOpenGLVertexArrayObject edgeVao_;
	QOpenGLVertexArrayObject boundingBoxVao_;

	GLsizei triangleIndexCount_ = 0;
	GLsizei edgeIndexCount_ = 0;
	GLsizei boundingBoxIndexCount_ = 0;
};
