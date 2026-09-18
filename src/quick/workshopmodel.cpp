#include "workshopmodel.h"

#include <QByteArray>
#include <QVector3D>
#include <cfloat>
#include <cstring>

static QByteArray floatsToByteArray(const QVector<float>& a)
{
	QByteArray b;
	b.resize(a.size() * sizeof(float));
	if (!a.isEmpty())
		memcpy(b.data(), a.constData(), b.size());
	return b;
}

WorkshopModel::WorkshopModel(QQuick3DObject* parent)
	: QQuick3DGeometry(parent)
{
}

void WorkshopModel::setVertexByteArray(const QByteArray& data, int stride)
{
	if (data.isEmpty())
		return;
	m_vertexData = data;
	m_stride = stride;
	clear();
	setStride(stride);
	addAttribute(Attribute::PositionSemantic, 0, Attribute::F32Type);
	setVertexData(data);
	update();
}

void WorkshopModel::setIndexByteArray(const QByteArray& data)
{
	if (data.isEmpty())
		return;
	addAttribute(Attribute::IndexSemantic, 0, Attribute::U16Type);
	setIndexData(data);
	setPrimitiveType(PrimitiveType::Triangles);
	update();
}

void WorkshopModel::updateFromArrays(const QVector<float>& verts, const QVector<size_t>& indices)
{
	if (!verts.isEmpty())
		setVertexByteArray(floatsToByteArray(verts), 3 * sizeof(float));
	if (!indices.isEmpty())
	{
		m_indices = indices;
		QByteArray idx16;
		idx16.resize(indices.size() * sizeof(uint16_t));
		auto* dst = reinterpret_cast<uint16_t*>(idx16.data());
		for (int i = 0; i < indices.size(); ++i)
			dst[i] = static_cast<uint16_t>(indices[i]);
		setIndexByteArray(idx16);
	}
}
