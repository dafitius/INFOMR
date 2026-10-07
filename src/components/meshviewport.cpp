#include "meshviewport.h"

#include <QOpenGLContext>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace
{
	constexpr float orbitDegreesPerPixel = 0.4F;
	constexpr float zoomFactorPerWheelStep = 0.9F;
	constexpr float degreesPerWheelStep = 15.0F;
} // namespace

MeshViewport::MeshViewport(QWidget* parent)
	: QOpenGLWidget(parent)
{
	setFocusPolicy(Qt::StrongFocus);
}

MeshViewport::~MeshViewport()
{
	releaseGpuResources();
}

void MeshViewport::setMesh(const Mesh& mesh)
{
	const auto centeredMesh = mesh.translated(-mesh.bary_center());
	auto alignedMesh = centeredMesh.rotated(centeredMesh.principle_axes());

	camera_.frame(mesh_->bounds());

	uploadPending_ = true;
	update();
}

void MeshViewport::setRenderSettings(const RenderSettings& settings)
{
	settings_ = settings;
	update();
}

const RenderSettings& MeshViewport::renderSettings() const
{
	return settings_;
}

void MeshViewport::resetView()
{
	camera_.reset();
	update();
}

void MeshViewport::initializeGL()
{
	initializeOpenGLFunctions();

	connect(context(), &QOpenGLContext::aboutToBeDestroyed, this, &MeshViewport::releaseGpuResources, Qt::UniqueConnection);

	renderer_ = MeshRenderer::init();
	uploadPending_ = mesh_.has_value();
}

void MeshViewport::paintGL()
{
	glClearColor(0x11 / 255.0F, 0x13 / 255.0F, 0x18 / 255.0F, 1.0F);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if (uploadPending_)
	{
		gpuMesh_ = mesh_ ? std::make_unique<GpuMesh>(*mesh_) : nullptr;
		uploadPending_ = false;
	}

	if (!renderer_ || !gpuMesh_)
	{
		return;
	}

	const float aspectRatio = static_cast<float>(width()) / static_cast<float>(std::max(height(), 1));
	renderer_->draw(*gpuMesh_, camera_, aspectRatio, settings_);
}

void MeshViewport::resizeGL(const int width, const int height)
{
	glViewport(0, 0, width, height);
}

void MeshViewport::mousePressEvent(QMouseEvent* event)
{
	lastMousePosition_ = event->position().toPoint();
}

void MeshViewport::mouseMoveEvent(QMouseEvent* event)
{
	const QPoint position = event->position().toPoint();
	const QPoint delta = position - lastMousePosition_;
	lastMousePosition_ = position;

	const bool panning =
		event->buttons() & (Qt::RightButton | Qt::MiddleButton) || (event->buttons() & Qt::LeftButton && event->modifiers() & Qt::ShiftModifier);

	if (panning)
	{
		const float unitsPerPixel = camera_.worldUnitsPerPixel(height());
		camera_.pan(-static_cast<float>(delta.x()) * unitsPerPixel, static_cast<float>(delta.y()) * unitsPerPixel);
	}
	else if (event->buttons() & Qt::LeftButton)
	{
		camera_.orbit(-static_cast<float>(delta.x()) * orbitDegreesPerPixel, static_cast<float>(delta.y()) * orbitDegreesPerPixel);
	}
	else
	{
		return;
	}

	update();
}

void MeshViewport::mouseDoubleClickEvent(QMouseEvent* /*event*/)
{
	resetView();
}

void MeshViewport::wheelEvent(QWheelEvent* event)
{
	const float steps = static_cast<float>(event->angleDelta().y()) / 8.0F / degreesPerWheelStep;
	camera_.zoom(std::pow(zoomFactorPerWheelStep, steps));
	update();
}

void MeshViewport::releaseGpuResources()
{
	if (!gpuMesh_ && !renderer_)
	{
		return;
	}

	makeCurrent();
	gpuMesh_.reset();
	renderer_.reset();
	doneCurrent();

	uploadPending_ = mesh_.has_value();
}
