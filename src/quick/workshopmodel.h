#pragma once

#include <QtQuick3D/QQuick3DGeometry>

class WorkshopModel : public QQuick3DGeometry
{
	Q_OBJECT
public:
	explicit WorkshopModel(QQuick3DObject* parent = nullptr);

	Q_INVOKABLE void setVertexByteArray(const QByteArray& data, int stride = 3 * sizeof(float));
	Q_INVOKABLE void setIndexByteArray(const QByteArray& data);
	Q_INVOKABLE void updateFromArrays(const QVector<float>& verts, const QVector<size_t>& indices);

private:
	QVector<size_t> m_indices;
	QByteArray m_vertexData;
	int m_stride = 0;
	bool m_wireframe = false;
};
