#include "meshrenderer.h"

#include <QMatrix4x4>
#include <algorithm>
#include <array>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

std::optional<MeshRenderer> MeshRenderer::init() {
	auto surfaceProgram = ShaderProgram::init(":/shaders/mesh.vert", ":/shaders/mesh.frag");
	auto edgeProgram = ShaderProgram::init(":/shaders/edge.vert", ":/shaders/edge.frag");

	if (!surfaceProgram || !edgeProgram) {
		return std::nullopt;
	}

	auto self = MeshRenderer();
	self.initializeOpenGLFunctions();
	self.surfaceProgram_ = std::move(surfaceProgram.value());
	self.edgeProgram_ = std::move(edgeProgram.value());
	return self;
}

void MeshRenderer::draw(GpuMesh& mesh, const Camera& camera, const float aspectRatio, const RenderSettings& settings, const glm::quat& eigenAxes) {
	const glm::mat4 view = camera.viewMatrix();
	const glm::mat4 projection = camera.projectionMatrix(aspectRatio);
	const glm::mat4 mvp = projection * view;

	glEnable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

	if (settings.showGrid) {
		constexpr QVector3D gridColor = {1.0f, 1.0f, 1.0f};

		edgeProgram_.bound([&](QOpenGLShaderProgram& program) {
			glUniformMatrix4fv(program.uniformLocation("mvp"), 1, GL_FALSE, glm::value_ptr(mvp));
			program.setUniformValue("edgeColor", gridColor);
			mesh.grid_.bound([&] { glDrawElements(GL_LINES, mesh.grid_.indexCount(), GL_UNSIGNED_INT, nullptr); });
		});
	}

	if (settings.showSurface) {
		// Push the surface slightly back so the edges drawn on top of it don't z-fight.
		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(1.0F, 1.0F);

		surfaceProgram_.bound([&](QOpenGLShaderProgram& program) {
			glUniformMatrix4fv(program.uniformLocation("mvp"), 1, GL_FALSE, glm::value_ptr(mvp));
			glUniformMatrix4fv(program.uniformLocation("modelView"), 1, GL_FALSE, glm::value_ptr(view));
			program.setUniformValue("fillColor", settings.surfaceColor);
			mesh.triangleMesh_.bound([&] { glDrawElements(GL_TRIANGLES, mesh.triangleMesh_.indexCount(), GL_UNSIGNED_INT, nullptr); });
		});

		glDisable(GL_POLYGON_OFFSET_FILL);
	}

	if (settings.showEdges) {
		const QVector3D edgeColor = settings.showSurface ? settings.edgeColor : settings.surfaceColor;

		edgeProgram_.bound([&](QOpenGLShaderProgram& program) {
			glUniformMatrix4fv(program.uniformLocation("mvp"), 1, GL_FALSE, glm::value_ptr(mvp));
			program.setUniformValue("edgeColor", {1.0f, 0.0f, 0.0f});
			mesh.boundaryEdges_.bound([&] { glDrawElements(GL_LINES, mesh.boundaryEdges_.indexCount(), GL_UNSIGNED_INT, nullptr); });
		});

		edgeProgram_.bound([&](QOpenGLShaderProgram& program) {
			glUniformMatrix4fv(program.uniformLocation("mvp"), 1, GL_FALSE, glm::value_ptr(mvp));
			program.setUniformValue("edgeColor", edgeColor);
			mesh.edgeMesh_.bound([&] { glDrawElements(GL_LINES, mesh.edgeMesh_.indexCount(), GL_UNSIGNED_INT, nullptr); });
		});
	}

	if (settings.showBoundingBox) {
		edgeProgram_.bound([&](QOpenGLShaderProgram& program) {
			glUniformMatrix4fv(program.uniformLocation("mvp"), 1, GL_FALSE, glm::value_ptr(mvp));
			program.setUniformValue("edgeColor", QVector3D(1.0F, 0.65F, 0.1F));

			mesh.boundingBox_.bound([&] { glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, nullptr); });
		});
	}

	if (settings.showNormals) {
		constexpr QVector3D normalColor = QVector3D(1.0F, 0.65F, 0.1F);

		edgeProgram_.bound([&](QOpenGLShaderProgram& program) {
			glUniformMatrix4fv(program.uniformLocation("mvp"), 1, GL_FALSE, glm::value_ptr(mvp));
			program.setUniformValue("edgeColor", normalColor);
			mesh.vertexNormals_.bound([&] { glDrawElements(GL_LINES, mesh.vertexNormals_.indexCount(), GL_UNSIGNED_INT, nullptr); });
		});
	}

	if (settings.showAxes || settings.showEigenvectorAxes) {
		const float axisLength = 1;
		const glm::mat4 axisScale = glm::scale(glm::mat4(1.0F), glm::vec3(axisLength));
		const std::array<QVector3D, 3> worldColors = {
			QVector3D(1.0F, 0.2F, 0.2F),
			QVector3D(0.2F, 1.0F, 0.2F),
			QVector3D(0.2F, 0.5F, 1.0F),
		};
		const std::array<QVector3D, 3> eigenColors = {
			QVector3D(1.0F, 0.2F, 0.8F),
			QVector3D(0.2F, 1.0F, 1.0F),
			QVector3D(1.0F, 0.8F, 0.2F),
		};

		glDisable(GL_DEPTH_TEST);
		edgeProgram_.bound([&](QOpenGLShaderProgram& program) {
			glLineWidth(3);
			for (int axis = 0; axis < 3; ++axis) {
				if (settings.showAxes) {
					glUniformMatrix4fv(program.uniformLocation("mvp"), 1, GL_FALSE, glm::value_ptr(projection * view * axisScale));
					program.setUniformValue("edgeColor", worldColors[axis]);
					mesh.axis_.bound([&] { glDrawArrays(GL_LINES, axis * 2, 2); });
				}

				if (settings.showEigenvectorAxes) {
					const glm::mat4 eigenTransform = glm::mat4_cast(eigenAxes) * axisScale;
					glUniformMatrix4fv(program.uniformLocation("mvp"), 1, GL_FALSE, glm::value_ptr(projection * view * eigenTransform));
					program.setUniformValue("edgeColor", eigenColors[axis]);
					mesh.axis_.bound([&] { glDrawArrays(GL_LINES, axis * 2, 2); });
				}
			}
			glLineWidth(1);
		});
		glEnable(GL_DEPTH_TEST);
	}

	if (settings.showHull) {
		constexpr auto hullColor = QVector3D(0.1F, 0.65F, 0.1F);

		edgeProgram_.bound([&](QOpenGLShaderProgram& program) {
			glUniformMatrix4fv(program.uniformLocation("mvp"), 1, GL_FALSE, glm::value_ptr(mvp));
			program.setUniformValue("edgeColor", hullColor);
			mesh.hull_.bound([&] { glDrawElements(GL_LINES, mesh.hull_.indexCount(), GL_UNSIGNED_INT, nullptr); });
		});
	}

	if (settings.showCurvature) {
		surfaceProgram_.bound([&](QOpenGLShaderProgram& program) {
			glUniformMatrix4fv(program.uniformLocation("mvp"), 1, GL_FALSE, glm::value_ptr(mvp));
			glUniformMatrix4fv(program.uniformLocation("modelView"), 1, GL_FALSE, glm::value_ptr(view));


			auto curvatureColor = [](float value) -> QVector3D {
				if (!std::isfinite(value))
					return {0.5F, 0.5F, 0.5F}; // skipped faces: grey

				constexpr std::size_t numBins = 128;
				constexpr std::array colors = {
					QVector3D{0.0F, 0.0F, 1.0F}, // blue
					QVector3D{0.0F, 1.0F, 0.0F}, // green
					QVector3D{0.0F, 1.0F, 1.0F}, // cyan
					QVector3D{1.0F, 0.0F, 0.0F}, // red
					QVector3D{1.0F, 0.0F, 1.0F}	 // magenta
				};

				const auto bin = std::min(static_cast<std::size_t>(std::clamp(value, 0.0F, 1.0F) * numBins), numBins - 1);
				return colors[bin * colors.size() / numBins];
			};


			for (std::size_t face = 0; face < mesh.curvatureValues_.size(); ++face) {
				program.setUniformValue("fillColor", curvatureColor(mesh.curvatureValues_[face]));

				const auto offset = reinterpret_cast<const void*>(face * 3 * sizeof(std::uint32_t));

				mesh.triangleMesh_.bound([&] { glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, offset); });
			}
		});
	}
}
