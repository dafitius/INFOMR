#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include "rendermesh.h"

class Camera
{
public:
	void frame(const Bounds& bounds);
	void reset();

	void orbit(float deltaYaw, float deltaPitch);
	void pan(float deltaX, float deltaY);
	void zoom(float factor);

	[[nodiscard]] QMatrix4x4 viewMatrix() const;
	[[nodiscard]] QMatrix4x4 projectionMatrix(float aspectRatio) const;
	[[nodiscard]] float worldUnitsPerPixel(int viewportHeight) const;

private:
	[[nodiscard]] QVector3D position() const;

	static constexpr float fieldOfView_ = 45.0F;
	static constexpr float defaultYaw_ = 30.0F;
	static constexpr float defaultPitch_ = 20.0F;

	Bounds framedBounds_ = {QVector3D(-1.0F, -1.0F, -1.0F), QVector3D(1.0F, 1.0F, 1.0F)};

	QVector3D target_;
	float distance_ = 1.0F;
	float yaw_ = defaultYaw_;
	float pitch_ = defaultPitch_;
};
