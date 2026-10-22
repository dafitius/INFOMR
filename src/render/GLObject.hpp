//
// Created by dafitius on 15-10-2026.
//

#ifndef MULTIMEDIARETRIEVAL_GLOBJECT_HPP
#define MULTIMEDIARETRIEVAL_GLOBJECT_HPP
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLFunctions>

class GlObject : protected QOpenGLFunctions
{
	public:
	QOpenGLBuffer vertex_ = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
	QOpenGLBuffer index_ = QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);

private:
	QOpenGLVertexArrayObject vao_;
	GLsizei indexCount_ = 0;

public:
	GlObject()
	{
		initializeOpenGLFunctions();
		vao_.create();
	};

	[[nodiscard]] GLsizei indexCount() const
	{
		return indexCount_;
	}

	template <typename Func_>
	void bound(Func_ func)
	{
		vao_.bind();
		func();
		vao_.release();
	};

	template <typename B, typename T>
		requires std::convertible_to<B, QOpenGLBuffer>
	void allocate(B GlObject::* bufferType, const std::span<T> data)
	{
		if (bufferType == &GlObject::index_)
		{
			indexCount_ = static_cast<int>(data.size());
		}
		QOpenGLBuffer& buffer = this->*bufferType;
		buffer.create();
		buffer.bind();
		buffer.setUsagePattern(QOpenGLBuffer::StaticDraw);
		buffer.allocate(data.data(), static_cast<int>(data.size() * sizeof(T)));
		configureVertexArrayObject();
	}

	template <typename B, typename T>
		requires std::convertible_to<B, QOpenGLBuffer>
	void allocate(B GlObject::* bufferType, const std::vector<T>& data)
	{
		allocate(bufferType, std::span{data});
	}

	template <typename B>
	requires std::convertible_to<B, QOpenGLBuffer>
	void allocate(B GlObject::* bufferType, GlObject& other)
	{
		if (bufferType == &GlObject::index_)
		{
			indexCount_ = other.indexCount();
		}
		QOpenGLBuffer& buffer = this->*bufferType;
		const QOpenGLBuffer& otherBuffer = other.*bufferType;
		buffer = otherBuffer;
		configureVertexArrayObject();
	}

	void configureVertexArrayObject()
	{
		this->bound([&]
		{
			vertex_.bind();
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
			index_.bind();
		});

		vertex_.release();
		index_.release();
	}


};

#endif // MULTIMEDIARETRIEVAL_GLOBJECT_HPP
