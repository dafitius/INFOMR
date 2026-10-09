#include "browsercard.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "meshpreview.h"

BrowserCard::BrowserCard(const QString& name, const std::filesystem::path& meshSource, QWidget* parent)
	: QFrame(parent)
	, meshName(name)
{
	// TODO: This is somewhat duplicate code with mainwindow

	if (meshSource.extension() == ".obj")
	{
		object.emplace(*Mesh::fromObj(meshSource));
	}
	else
	{
		std::cerr << "Unsupported mesh format: " << meshSource.extension() << '\n';
		return;
	}

	if (!object)
	{
		std::cerr << "Failed to load mesh: " << meshSource << '\n';
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

	const auto mesh = object->translated(-object->baryCenter()).filledHoles();
	object.emplace(mesh);

	const auto mesh2 = object->rotated(object->principleAxes());
	object.emplace(mesh2);

	auto* preview = new MeshPreview(object->vertexPositionBuffer(), object->triangleIndices(), this);
	preview->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	preview->setMinimumHeight(180);
	mainLayout->addWidget(preview, 1);

	auto* nameLabel = new QLabel(meshName, this);
	nameLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
	mainLayout->addWidget(nameLabel);

	auto* statsLabel = new QLabel(QString("Vertices: %1 | Faces: %2").arg(object->numVertices()).arg(object->numFaces()), this);
	statsLabel->setStyleSheet("color: #666; font-size: 11px;");
	mainLayout->addWidget(statsLabel);

	auto* inspectBtn = new QPushButton("Inspect", this);
	inspectBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	mainLayout->addWidget(inspectBtn);

	connect(inspectBtn, &QPushButton::clicked, this, [this] { emit meshClicked(meshName); });
}