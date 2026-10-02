#pragma once

#include "../io/mesh.hpp"
#include <glm/matrix.hpp>
#include <glm/trigonometric.hpp>

class Camera
{
public:
	void frame(const Mesh::Bounds& bounds);
	void reset();

	void orbit(float deltaYaw, float deltaPitch);
	void pan(float deltaX, float deltaY);
	void zoom(float factor);

	[[nodiscard]] glm::mat4 viewMatrix() const;
	[[nodiscard]] glm::mat4 projectionMatrix(float aspectRatio) const;
	[[nodiscard]] float worldUnitsPerPixel(int viewportHeight) const;

private:
	[[nodiscard]] glm::vec3 position() const;

	static constexpr float fieldOfView_ = 45.0F;
	static constexpr float defaultYaw_ = 30.0F;
	static constexpr float defaultPitch_ = 20.0F;

	Mesh::Bounds framedBounds_ = {.min = glm::vec3(-1.0F, -1.0F, -1.0F), .max = glm::vec3(1.0F, 1.0F, 1.0F)};

	glm::vec3 target_;
	float distance_ = 1.0F;
	float yaw_ = defaultYaw_;
	float pitch_ = defaultPitch_;
};
