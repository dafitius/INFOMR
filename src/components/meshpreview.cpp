#include "meshpreview.h"

#include <QMatrix4x4>
#include <QSizePolicy>

#include <algorithm>
#include <vector>

MeshPreview::MeshPreview(const std::span<const float> vertexBuffer, const std::span<const uint32_t> indexBuffer, QWidget* parent)
	: QOpenGLWidget(parent)
	, vertex_buffer(vertexBuffer)
	, index_buffer(indexBuffer)
{
	setMinimumSize(180, 140);
	setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	clock_.start();

	connect(&frameTimer_, &QTimer::timeout, this, QOverload<>::of(&MeshPreview::update));

	// Approximately 60 frames per second.
	frameTimer_.start(16);
}

void MeshPreview::setRotationSpeed(const float rotationSpeed)
{
	rotation_speed_ = rotationSpeed;
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

	program_ = ShaderProgram::init(":/shaders/preview.vert", ":/shaders/preview.frag").value();

	glEnable(GL_DEPTH_TEST);

	const auto vertices = vertex_buffer;
	const auto sourceIndices = index_buffer;

	auto indices = std::vector<GLuint>(sourceIndices.size());

	std::ranges::transform(sourceIndices, indices.begin(), [](const size_t index) { return static_cast<GLuint>(index); });

	indexCount_ = static_cast<GLsizei>(indices.size());

	vao_.create();
	vao_.bind();

	// Vertex buffer
	glGenBuffers(1, &vertexBuffer_);
	glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);

	glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)), vertices.data(), GL_STATIC_DRAW);

	auto configureVertexAttributes = [](QOpenGLShaderProgram& prog)
	{
		prog.enableAttributeArray(0);
		prog.setAttributeBuffer(0, GL_FLOAT, 0, 3, 3 * static_cast<int>(sizeof(float)));
	};

	program_.bound(configureVertexAttributes);

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

	const float aspectRatio = static_cast<float>(width()) / static_cast<float>(height());

	const float elapsedSeconds = static_cast<float>(clock_.elapsed()) / 1000.0F;

	const float rotationAngle = elapsedSeconds * rotation_speed_;

	QMatrix4x4 model;
	model.rotate(30.0F, 1.0F, 0.0F, 0.0F);
	model.rotate(rotationAngle, 0.0F, 1.0F, 0.0F);

	QMatrix4x4 view;
	view.translate(0.0F, 0.0F, -2.5F);

	QMatrix4x4 projection;
	projection.perspective(45.0F, aspectRatio, 0.1F, 100.0F);

	auto renderLoop = [this, projection, view, model](QOpenGLShaderProgram& prog)
	{
		const auto mvpMatrix = projection * view * model;
		const auto modelViewMatrix = view * model;

		prog.setUniformValue("mvp", mvpMatrix);
		prog.setUniformValue("modelView", modelViewMatrix);

		vao_.bind();
		glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, nullptr);
		vao_.release();
	};

	program_.bound(renderLoop);
}