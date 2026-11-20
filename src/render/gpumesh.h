#pragma once

#include "GLObject.hpp"


#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLVertexArrayObject>

class Mesh;

// Owns the GPU buffers of a mesh. Must be created and destroyed while its OpenGL context is current.
class GpuMesh {
public:
	explicit GpuMesh(const Mesh& mesh);
	~GpuMesh();

	GpuMesh(const GpuMesh&) = delete;
	GpuMesh& operator=(const GpuMesh&) = delete;

	[[nodiscard]] float axisLength() const;

	static std::pair<std::vector<uint32_t>, std::vector<float>> generateGrid();
	static std::pair<std::vector<uint32_t>, std::vector<float>> generateNormals(const Mesh& mesh, float length);
	static std::pair<std::vector<uint32_t>, std::vector<float>> generateBoundaryEdges(const Mesh& mesh);
	static std::pair<std::vector<uint32_t>, std::vector<float>> generateBoundingBox(const Mesh& mesh);
	static std::vector<uint32_t> generateHullEdges(const Mesh& mesh);
	static std::vector<float> generateAxis();

	GlObject triangleMesh_;
	GlObject edgeMesh_;
	GlObject boundingBox_;
	GlObject grid_;
	GlObject hull_;
	GlObject vertexNormals_;
	GlObject boundaryEdges_;
	GlObject axis_;
	std::vector<float> curvatureValues_;

private:
	float axisLength_ = 1.0F;
};
