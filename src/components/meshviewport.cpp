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
	mesh_.emplace(mesh);

	uploadPending_ = true;
	update();
}

const std::optional<Mesh> MeshViewport::getMesh()
{
	return mesh_;
}

void MeshViewport::orientNormals()
{
	if (!mesh_)
		return;

	mesh_.emplace(mesh_->withConsistentOrientations());
	uploadPending_ = true;
	update();
}

void MeshViewport::fillHoles()
{
	if (!mesh_)
		return;

	mesh_.emplace(mesh_->filledHoles());
	uploadPending_ = true;
	update();
}

void MeshViewport::normalizeTranslation()
{
	if (!mesh_)
		return;

	mesh_.emplace(mesh_->translated(-mesh_->baryCenter()));
	uploadPending_ = true;
	update();
}

void MeshViewport::normalizeRotation()
{
	if (!mesh_)
		return;

	mesh_.emplace(mesh_->rotated(glm::inverse(mesh_->principleAxes())));
	uploadPending_ = true;
	update();
}

void MeshViewport::normalizePose()
{
	if (!mesh_)
		return;

	if (mesh_->moment().x < 0)
	{
		mesh_.emplace(mesh_->rotated(glm::quat{0, 0, 0, 1}));
		uploadPending_ = true;
		update();
	}
}

void MeshViewport::scaleToUnit()
{
	if (!mesh_)
		return;

	auto bounds = mesh_->bounds();
	// X axis should be the largest axis after aligning with eigen vectors
	auto scale = 1 / (bounds.max.x - bounds.min.x);
	mesh_.emplace(mesh_->scaled(glm::vec3{scale}));
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
	camera_.frame(mesh_->bounds());
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
	QColor const backgroundColor = palette().color(QPalette::Window);
	glClearColor(backgroundColor.redF(), backgroundColor.greenF(), backgroundColor.blueF(), backgroundColor.alphaF());
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
	renderer_->draw(*gpuMesh_, camera_, aspectRatio, settings_, mesh_->principleAxes());
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
