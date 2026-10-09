#include "meshrenderer.h"
#include <glm/gtc/type_ptr.hpp>

std::optional<MeshRenderer> MeshRenderer::init()
{
	auto surfaceProgram = ShaderProgram::init(":/shaders/mesh.vert", ":/shaders/mesh.frag");
	auto edgeProgram = ShaderProgram::init(":/shaders/edge.vert", ":/shaders/edge.frag");

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
	const glm::mat4 view = camera.viewMatrix();
	const glm::mat4 mvp = camera.projectionMatrix(aspectRatio) * view;

	glEnable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

	if (settings.showGrid)
	{
		constexpr QVector3D gridColor = {1.0f, 1.0f, 1.0f};

		edgeProgram_.bound(
			[&](QOpenGLShaderProgram& program)
			{
				glUniformMatrix4fv(program.uniformLocation("mvp"), 1, GL_FALSE, glm::value_ptr(mvp));
				program.setUniformValue("edgeColor", gridColor);
				mesh.drawGrid();
			});
	}

	if (settings.showSurface)
	{
		// Push the surface slightly back so the edges drawn on top of it don't z-fight.
		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(1.0F, 1.0F);

		surfaceProgram_.bound(
			[&](QOpenGLShaderProgram& program)
			{
				glUniformMatrix4fv(program.uniformLocation("mvp"), 1, GL_FALSE, glm::value_ptr(mvp));
				glUniformMatrix4fv(program.uniformLocation("modelView"), 1, GL_FALSE, glm::value_ptr(view));
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
				glUniformMatrix4fv(program.uniformLocation("mvp"), 1, GL_FALSE, glm::value_ptr(mvp));
				program.setUniformValue("edgeColor", {1.0f, 0.0f, 0.0f});
				mesh.drawBoundaryEdges();
			});

		edgeProgram_.bound(
			[&](QOpenGLShaderProgram& program)
			{
				glUniformMatrix4fv(program.uniformLocation("mvp"), 1, GL_FALSE, glm::value_ptr(mvp));
				program.setUniformValue("edgeColor", edgeColor);
				mesh.drawEdges();
			});
	}

	if (settings.showBoundingBox)
	{
		edgeProgram_.bound(
			[&](QOpenGLShaderProgram& program)
			{
				glUniformMatrix4fv(program.uniformLocation("mvp"), 1, GL_FALSE, glm::value_ptr(mvp));
				program.setUniformValue("edgeColor", QVector3D(1.0F, 0.65F, 0.1F));
				mesh.drawBoundingBox();
			});
	}

	if (settings.showNormals)
	{
		constexpr QVector3D normalColor = QVector3D(1.0F, 0.65F, 0.1F);

		edgeProgram_.bound(
			[&](QOpenGLShaderProgram& program)
			{
				glUniformMatrix4fv(program.uniformLocation("mvp"), 1, GL_FALSE, glm::value_ptr(mvp));
				program.setUniformValue("edgeColor", normalColor);
				mesh.drawNormals();
			});
	}
}
