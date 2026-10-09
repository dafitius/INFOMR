#pragma once

#include <QOpenGLShaderProgram>
#include <QString>

class ShaderProgram
{
public:
	ShaderProgram();

	ShaderProgram(ShaderProgram&& other) noexcept
		: program_(std::move(other.program_))
	{
	}

	ShaderProgram& operator=(ShaderProgram&& other) noexcept
	{
		if (this == &other)
			return *this;
		program_ = std::move(other.program_);
		return *this;
	}

	static std::optional<ShaderProgram> init(const QString& vertexPath, const QString& fragmentPath);

	void bound(const std::function<void(QOpenGLShaderProgram&)>&) const;

private:
	std::unique_ptr<QOpenGLShaderProgram> program_;
};