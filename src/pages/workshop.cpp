//
// Created by dafitius on 18-09-2026.
//

#include "workshop.h"

#include "../io/mesh.hpp"
#include <QCheckBox>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QMetaObject>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWidget>
#include <QSplitter>
#include <QUrl>
#include <QVBoxLayout>

Workshop::Workshop(QWidget* parent)
	: QWidget(parent)
{
	setupUI();
}

void Workshop::loadMesh(const Mesh& mesh)
{
	// Convert positions to QVector<float>
	const auto posSpan = mesh.positions();
	QVector<float> verts;
	verts.reserve(posSpan.size() * 3);
	for (const auto& p : posSpan)
	{
		verts.push_back(std::get<0>(p));
		verts.push_back(std::get<1>(p));
		verts.push_back(std::get<2>(p));
	}

	QVector<size_t> indices;
	indices.reserve(mesh.num_faces() * 3);
	for (const auto& face : mesh.faces())
	{
		for (auto idx : face)
			indices.push_back(static_cast<uint32_t>(idx));
	}

	if (viewWidget && viewWidget->status() == QQuickWidget::Ready)
	{
		if (QQuickItem* root = viewWidget->rootObject())
		{
			if (QObject* modelObj = root->findChild<QObject*>("workshopModel"))
			{
				QMetaObject::invokeMethod(modelObj, "updateFromArrays", Q_ARG(QVector<float>, verts), Q_ARG(QVector<size_t>, indices));
			}
			else
			{
				qDebug() << "Workshop: workshopModel not in root";
			}
		}
	}
}

Workshop::~Workshop() = default;

void Workshop::setupUI()
{
	auto* mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(16, 16, 16, 16);

	auto* splitter = new QSplitter(Qt::Horizontal, this);
	splitter->setChildrenCollapsible(false);
	mainLayout->addWidget(splitter);

	auto* viewContainer = new QWidget(splitter);
	viewContainer->setObjectName("workshopViewport");

	viewWidget = std::make_unique<QQuickWidget>(viewContainer);
	viewWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
	viewWidget->setSource(QUrl("qrc:/qml/WorkshopView.qml"));

	auto* vcLayout = new QVBoxLayout(viewContainer);
	vcLayout->setContentsMargins(0, 0, 0, 0);
	vcLayout->addWidget(viewWidget.get());

	auto* sidebar = new QWidget(splitter);
	sidebar->setObjectName("workshopSidebar");
	sidebar->setMinimumWidth(260);

	auto* sidebarLayout = new QVBoxLayout(sidebar);
	sidebarLayout->setContentsMargins(8, 8, 8, 8);
	sidebarLayout->setSpacing(12);

	auto* title = new QLabel("Workshop", sidebar);
	title->setObjectName("pageTitle");
	sidebarLayout->addWidget(title);

	auto* wireframeBox = new QCheckBox("Wireframe", sidebar);
	wireframeBox->setToolTip("Render model as wireframe");
	sidebarLayout->addWidget(wireframeBox);

	sidebarLayout->addStretch();

	splitter->setStretchFactor(0, 2);
	splitter->setStretchFactor(1, 1);
	splitter->setSizes({800, 300});
}