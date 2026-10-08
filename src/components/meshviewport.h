#pragma once

#include <QOpenGLFunctions>
#include <QOpenGLWidget>
#include <QPoint>

#include <memory>
#include <optional>

#include "../render/camera.h"
#include "../render/gpumesh.h"
#include "../render/meshrenderer.h"

class MeshViewport
	: public QOpenGLWidget
	, protected QOpenGLFunctions
{
	Q_OBJECT

public:
	explicit MeshViewport(QWidget* parent = nullptr);
	~MeshViewport() override;

	void setMesh(const Mesh& mesh);

	void setRenderSettings(const RenderSettings& settings);
	[[nodiscard]] const RenderSettings& renderSettings() const;

public slots:
	void resetView();

protected:
	void initializeGL() override;
	void paintGL() override;
	void resizeGL(int width, int height) override;

	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void mouseDoubleClickEvent(QMouseEvent* event) override;
	void wheelEvent(QWheelEvent* event) override;

private:
	void releaseGpuResources();

	std::optional<MeshRenderer> renderer_;
	std::unique_ptr<GpuMesh> gpuMesh_;

	// Kept on the CPU so the mesh can be re-uploaded when Qt recreates the OpenGL context.
	std::optional<Mesh> mesh_;
	bool uploadPending_ = false;

	Camera camera_;
	RenderSettings settings_;
	QPoint lastMousePosition_;
};
