#pragma once

#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLVertexArrayObject>

class Mesh;

// Owns the GPU buffers of a mesh. Must be created and destroyed while its OpenGL context is current.
class GpuMesh : protected QOpenGLFunctions
{
public:
	explicit GpuMesh(const Mesh& mesh);
	~GpuMesh();

	GpuMesh(const GpuMesh&) = delete;
	GpuMesh& operator=(const GpuMesh&) = delete;

	void drawTriangles();
	void drawEdges();
	void drawBoundingBox();
	void drawAxis(int axis);
	[[nodiscard]] float axisLength() const;
	void drawGrid();
	void drawNormals();
	void drawBoundaryEdges();
	void drawHullEdges();

	void allocateGrid();
	void allocateNormals(const Mesh& mesh, float length);
	void allocateBoundaryEdges(const Mesh& mesh);
	void allocateHullEdges(const Mesh& mesh);


private:
	void configureVertexArray(QOpenGLVertexArrayObject& vao, QOpenGLBuffer& vertexBuffer, QOpenGLBuffer& indexBuffer);

	QOpenGLBuffer vertexBuffer_ = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
	QOpenGLBuffer triangleIndexBuffer_ = QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
	QOpenGLBuffer edgeIndexBuffer_ = QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
	QOpenGLBuffer boundingBoxVertexBuffer_ = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
	QOpenGLBuffer boundingBoxIndexBuffer_ = QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
	QOpenGLBuffer debugGridVertexBuffer_ = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
	QOpenGLBuffer debugGridIndexBuffer_ = QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
	QOpenGLBuffer debugNormalsVertexBuffer_ = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
	QOpenGLBuffer debugNormalsIndexBuffer_ = QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
	QOpenGLBuffer boundaryEdgesVertexBuffer_ = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
	QOpenGLBuffer boundaryEdgesIndexBuffer_ = QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
	QOpenGLBuffer axisVertexBuffer_ = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);

	// hullEdgesVertexBuffer_ is the same as vertexBuffer_
	QOpenGLBuffer hullEdgesIndexBuffer_ = QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);

	QOpenGLVertexArrayObject triangleVao_;
	QOpenGLVertexArrayObject edgeVao_;
	QOpenGLVertexArrayObject boundingBoxVao_;
	QOpenGLVertexArrayObject debugGridVao_;
	QOpenGLVertexArrayObject debugNormalsVao_;
	QOpenGLVertexArrayObject boundaryEdgesVao_;
	QOpenGLVertexArrayObject hullEdgesVao_;
	QOpenGLVertexArrayObject axisVao_;

	GLsizei triangleIndexCount_ = 0;
	GLsizei edgeIndexCount_ = 0;
	GLsizei debugGridIndexCount_ = 0;
	GLsizei debugNormalsIndexCount_ = 0;
	GLsizei boundaryEdgesIndexCount_ = 0;
	GLsizei hullEdgesIndexCount_ = 0;
	float axisLength_ = 1.0F;
};
