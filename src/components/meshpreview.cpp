#include "meshpreview.h"

#include <QDebug>
#include <QMatrix4x4>
#include <QSizePolicy>

#include <algorithm>
#include <utility>
#include <vector>

#include "../io/off_file.hpp"

MeshPreview::MeshPreview(std::span<const float> vertex_buffer, std::span<const uint32_t> index_buffer, QWidget* parent)
	: QOpenGLWidget(parent)
	, vertex_buffer(vertex_buffer)
	, index_buffer(index_buffer)
{
	setMinimumSize(180, 140);
	setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	clock_.start();

	connect(&frameTimer_, &QTimer::timeout, this, QOverload<>::of(&MeshPreview::update));

	// Approximately 60 frames per second.
	frameTimer_.start(16);
}

void MeshPreview::setRotationSpeed(float rotation_speed)
{
	rotation_speed_ = rotation_speed;
}

float MeshPreview::rotationSpeed() const
{
	return rotation_speed_;
}

void MeshPreview::initializeGL()
{
	initializeOpenGLFunctions();

	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);

	program_ = ShaderProgram::Init(":/shaders/preview.vert", ":/shaders/preview.frag").value();

	glEnable(GL_DEPTH_TEST);

	const auto vertices = vertex_buffer;
	const auto source_indices = index_buffer;

	auto indices = std::vector<GLuint>(source_indices.size());

	std::ranges::transform(source_indices, indices.begin(), [](const size_t index) { return static_cast<GLuint>(index); });

	indexCount_ = static_cast<GLsizei>(indices.size());

	vao_.create();
	vao_.bind();

	// Vertex buffer
	glGenBuffers(1, &vertexBuffer_);
	glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);

	glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)), vertices.data(), GL_STATIC_DRAW);

	auto configure_vertex_attributes = [](QOpenGLShaderProgram& prog)
	{
		prog.enableAttributeArray(0);
		prog.setAttributeBuffer(0, GL_FLOAT, 0, 3, 3 * static_cast<int>(sizeof(float)));
	};

	program_.bound(configure_vertex_attributes);

	glGenBuffers(1, &indexBuffer_);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer_);

	glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size() * sizeof(GLuint)), indices.data(), GL_STATIC_DRAW);

	vao_.release();

	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void MeshPreview::resizeGL(const int width, const int height)
{
	glViewport(0, 0, width, height);
}

void MeshPreview::paintGL()
{
	glClearColor(0x11 / 255.0, 0x13 / 255.0, 0x18 / 255.0, 1.0F);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	const float aspect_ratio = static_cast<float>(width()) / static_cast<float>(height());

	const float elapsed_seconds = static_cast<float>(clock_.elapsed()) / 1000.0F;

	const float rotation_angle = elapsed_seconds * rotation_speed_;

	QMatrix4x4 model;
	model.rotate(30.0F, 1.0F, 0.0F, 0.0F);
	model.rotate(rotation_angle, 0.0F, 1.0F, 0.0F);

	QMatrix4x4 view;
	view.translate(0.0F, 0.0F, -2.5F);

	QMatrix4x4 projection;
	projection.perspective(45.0F, aspect_ratio, 0.1F, 100.0F);

	auto render_loop = [this, projection, view, model](QOpenGLShaderProgram& prog)
	{
		const auto mvp_matrix = projection * view * model;
		const auto model_view_matrix = view * model;

		prog.setUniformValue("mvp", mvp_matrix);
		prog.setUniformValue("modelView", model_view_matrix);

		vao_.bind();
		glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, nullptr);
		vao_.release();
	};

	program_.bound(render_loop);
}