#pragma once

#include <optional>

#include "camera.h"
#include "gpumesh.h"
#include "shaderprogram.h"

struct RenderSettings
{
	bool showSurface = true;
	bool showEdges = false;
	bool showAxes = false;
	bool showEigenvectorAxes = false;
	bool showBoundingBox = false;
	bool showGrid = false;
	bool showNormals = false;
	bool showHull = false;
	bool showCurvature = false;
	QVector3D surfaceColor = QVector3D(0.25F, 0.65F, 0.95F);
	QVector3D edgeColor = QVector3D(0.05F, 0.06F, 0.08F);
};

class MeshRenderer : protected QOpenGLFunctions
{
public:
	static std::optional<MeshRenderer> init();

	void draw(GpuMesh& mesh, const Camera& camera, float aspectRatio, const RenderSettings& settings, const glm::quat& eigenAxes);

private:
	MeshRenderer() = default;

	ShaderProgram surfaceProgram_;
	ShaderProgram edgeProgram_;
};
