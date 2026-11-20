#include "shaderprogram.h"

#include <QFile>
#include <QOpenGLShaderProgram>
#include <QTextStream>

ShaderProgram::ShaderProgram() {
	this->program_ = std::make_unique<QOpenGLShaderProgram>();
}

std::optional<ShaderProgram> ShaderProgram::init(const QString& vertexPath, const QString& fragmentPath) {
	auto self = ShaderProgram();

	auto readResource = [](const QString& path) -> std::optional<QString> {
		QFile file(path);

		if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
			qWarning("Could not open shader: %s", qPrintable(path));
			return std::nullopt;
		}

		return QTextStream(&file).readAll();
	};

	const auto vertexSource = readResource(vertexPath);
	const auto fragmentSource = readResource(fragmentPath);

	if (!vertexSource || !fragmentSource) {
		return {};
	}

	if (!self.program_->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexSource.value())) {
		qWarning("Vertex shader compilation failed:\n%s", qPrintable(self.program_->log()));
		return {};
	}

	if (!self.program_->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentSource.value())) {
		qWarning("Fragment shader compilation failed:\n%s", qPrintable(self.program_->log()));
		return {};
	}

	if (!self.program_->link()) {
		qWarning("Shader linking failed:\n%s", qPrintable(self.program_->log()));
		return {};
	}
	return self;
}

void ShaderProgram::bound(const std::function<void(QOpenGLShaderProgram&)>& function) const {
	program_->bind();
	function(*program_);
	program_->release();
}
