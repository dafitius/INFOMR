#include "camera.h"

#include <QtMath>

#include <algorithm>
#include <cmath>

namespace
{
	const QVector3D worldUp = QVector3D(0.0F, 1.0F, 0.0F);
}

void Camera::frame(const Bounds& bounds)
{
	framedBounds_ = bounds;
	reset();
}

void Camera::reset()
{
	const float radius = std::max(framedBounds_.radius(), 1e-6F);

	target_ = framedBounds_.center();
	distance_ = radius / std::sin(qDegreesToRadians(fieldOfView_ * 0.5F)) * 1.1F;
	yaw_ = defaultYaw_;
	pitch_ = defaultPitch_;
}

void Camera::orbit(const float deltaYaw, const float deltaPitch)
{
	// Stay just short of the poles, which would cause a lock of camera rotation.
	yaw_ = std::fmod(yaw_ + deltaYaw, 360.0F);
	pitch_ = std::clamp(pitch_ + deltaPitch, -89.0F, 89.0F);
}

void Camera::pan(const float deltaX, const float deltaY)
{
	const QVector3D forward = (target_ - position()).normalized();
	const QVector3D right = QVector3D::crossProduct(forward, worldUp).normalized();
	const QVector3D up = QVector3D::crossProduct(right, forward);

	target_ += right * deltaX + up * deltaY;
}

void Camera::zoom(const float factor)
{
	const float minimumDistance = std::max(framedBounds_.radius(), 1e-6F) * 1e-3F;
	distance_ = std::max(distance_ * factor, minimumDistance);
}

QMatrix4x4 Camera::viewMatrix() const
{
	QMatrix4x4 view;
	view.lookAt(position(), target_, worldUp);
	return view;
}

QMatrix4x4 Camera::projectionMatrix(const float aspectRatio) const
{
	const float nearPlane = distance_ * 0.01F;
	const float farPlane = distance_ + framedBounds_.radius() * 2.0F;

	QMatrix4x4 projection;
	projection.perspective(fieldOfView_, aspectRatio, nearPlane, farPlane);
	return projection;
}

float Camera::worldUnitsPerPixel(const int viewportHeight) const
{
	const float visibleHeight = 2.0F * distance_ * std::tan(qDegreesToRadians(fieldOfView_ * 0.5F));
	return visibleHeight / static_cast<float>(std::max(viewportHeight, 1));
}

QVector3D Camera::position() const
{
	const float yaw = qDegreesToRadians(yaw_);
	const float pitch = qDegreesToRadians(pitch_);

	const auto direction = QVector3D(std::cos(pitch) * std::sin(yaw), std::sin(pitch), std::cos(pitch) * std::cos(yaw));
	return target_ + direction * distance_;
}
