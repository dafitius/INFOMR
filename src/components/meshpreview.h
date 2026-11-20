#pragma once

#include <QElapsedTimer>
#include <QOpenGLFunctions>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>
#include <QTimer>

#include "../render/shaderprogram.h"

class MeshPreview
	: public QOpenGLWidget
	, protected QOpenGLFunctions {
	Q_OBJECT

public:
	explicit MeshPreview(std::span<const float> vertexBuffer, std::span<const uint32_t> indexBuffer, QWidget* parent = nullptr);

	void setRotationSpeed(float rotationSpeed);
	[[nodiscard]] float rotationSpeed() const;

protected:
	void initializeGL() override;
	void paintGL() override;
	void resizeGL(int width, int height) override;

private:
	ShaderProgram program_;
	std::span<const float> vertex_buffer;
	std::span<const uint32_t> index_buffer;

	QOpenGLVertexArrayObject vao_;

	GLuint vertexBuffer_ = 0;
	GLuint indexBuffer_ = 0;
	GLsizei indexCount_ = 0;

	float rotation_speed_ = 30.0F;
	QElapsedTimer clock_;
	QTimer frameTimer_;
};