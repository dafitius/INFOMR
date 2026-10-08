#include "browsercard.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "../io/off_file.hpp"
#include "meshpreview.h"

BrowserCard::BrowserCard(const QString& name, const QString mesh_source, QWidget* parent)
	: QFrame(parent)
	, meshName(name)
{
	// TODO: This is somewhat duplicate code with mainwindow
	const std::filesystem::path path = mesh_source.toStdString();

	if (path.extension() == ".obj")
	{
		object.emplace(*Mesh::from_obj(path));
	}
	else
	{
		std::cerr << "Unsupported mesh format: " << path.extension() << '\n';
		return;
	}

	if (!object)
	{
		std::cerr << "Failed to load mesh: " << path << '\n';
		return;
	}

	setupUI();
}

BrowserCard::~BrowserCard() {}

void BrowserCard::setupUI()
{
	setObjectName("BrowserCard");
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

	setMinimumSize(260, 300);

	auto* mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(12, 12, 12, 12);
	mainLayout->setSpacing(10);

	const auto mesh = object->translated(-object->bary_center()).filled_holes();
	object.emplace(mesh);

	auto* preview = new MeshPreview(object->vertex_position_buffer(), object->triangle_indices(), this);
	preview->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	preview->setMinimumHeight(180);
	mainLayout->addWidget(preview, 1);

	auto* nameLabel = new QLabel(meshName, this);
	nameLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
	mainLayout->addWidget(nameLabel);

	auto* statsLabel = new QLabel(QString("Vertices: %1 | Faces: %2").arg(object->num_vertices()).arg(object->num_faces()), this);
	statsLabel->setStyleSheet("color: #666; font-size: 11px;");
	mainLayout->addWidget(statsLabel);

	auto* inspectBtn = new QPushButton("Inspect", this);
	inspectBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	mainLayout->addWidget(inspectBtn);

	connect(inspectBtn, &QPushButton::clicked, this, [this]() { emit meshClicked(meshName); });
}