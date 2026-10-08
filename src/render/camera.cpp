#include "camera.h"

#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

namespace
{
	constexpr auto worldUp = glm::vec3(0.0F, 1.0F, 0.0F);
}

void Camera::frame(const Mesh::Bounds& bounds)
{
	framedBounds_ = bounds;
	reset();
}

void Camera::reset()
{
	const float radius = std::max(framedBounds_.radius(), 1e-6F);

	target_ = {0,0,0};//framedBounds_.center();
	distance_ = radius / std::sin(glm::radians(fieldOfView_ * 0.5F)) * 1.1F;
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
	const glm::vec3 forward = glm::normalize(target_ - position());
	const glm::vec3 right = glm::normalize(glm::cross(forward, worldUp));
	const glm::vec3 up = glm::cross(right, forward);

	target_ += right * deltaX + up * deltaY;
}

void Camera::zoom(const float factor)
{
	const float minimumDistance = std::max(framedBounds_.radius(), 1e-6F) * 1e-3F;
	distance_ = std::max(distance_ * factor, minimumDistance);
}

glm::mat4 Camera::viewMatrix() const
{
	return glm::lookAt(position(), target_, worldUp);
	;
}

glm::mat4 Camera::projectionMatrix(const float aspectRatio) const
{
	const float nearPlane = distance_ * 0.01F;
	const float farPlane = distance_ + framedBounds_.radius() * 20.0F;
	return glm::perspective(glm::radians(fieldOfView_), aspectRatio, nearPlane, farPlane);
}

float Camera::worldUnitsPerPixel(const int viewportHeight) const
{
	const float visibleHeight = 2.0F * distance_ * std::tan(glm::radians(fieldOfView_ * 0.5F));
	return visibleHeight / static_cast<float>(std::max(viewportHeight, 1));
}

glm::vec3 Camera::position() const
{
	const float yaw = glm::radians(yaw_);
	const float pitch = glm::radians(pitch_);

	const auto direction = glm::vec3(std::cos(pitch) * std::sin(yaw), std::sin(pitch), std::cos(pitch) * std::cos(yaw));
	return target_ + direction * distance_;
}
