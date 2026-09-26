#include "meshrenderer.h"

#include <QMatrix4x4>

std::optional<MeshRenderer> MeshRenderer::Init()
{
	auto surfaceProgram = ShaderProgram::Init(":/shaders/mesh.vert", ":/shaders/mesh.frag");
	auto edgeProgram = ShaderProgram::Init(":/shaders/edge.vert", ":/shaders/edge.frag");

	if (!surfaceProgram || !edgeProgram)
	{
		return std::nullopt;
	}

	auto self = MeshRenderer();
	self.initializeOpenGLFunctions();
	self.surfaceProgram_ = std::move(surfaceProgram.value());
	self.edgeProgram_ = std::move(edgeProgram.value());
	return self;
}

void MeshRenderer::draw(GpuMesh& mesh, const Camera& camera, const float aspectRatio, const RenderSettings& settings)
{
	const QMatrix4x4 view = camera.viewMatrix();
	const QMatrix4x4 mvp = camera.projectionMatrix(aspectRatio) * view;

	glEnable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

	if (settings.showSurface)
	{
		// Push the surface slightly back so the edges drawn on top of it don't z-fight.
		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(1.0F, 1.0F);

		surfaceProgram_.bound(
			[&](QOpenGLShaderProgram& program)
			{
				program.setUniformValue("mvp", mvp);
				program.setUniformValue("modelView", view);
				program.setUniformValue("fillColor", settings.surfaceColor);
				mesh.drawTriangles();
			});

		glDisable(GL_POLYGON_OFFSET_FILL);
	}

	if (settings.showEdges)
	{
		const QVector3D edgeColor = settings.showSurface ? settings.edgeColor : settings.surfaceColor;

		edgeProgram_.bound(
			[&](QOpenGLShaderProgram& program)
			{
				program.setUniformValue("mvp", mvp);
				program.setUniformValue("edgeColor", edgeColor);
				mesh.drawEdges();
			});
	}
}
