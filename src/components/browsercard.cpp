#include "browsercard.h"

#include "meshpreview.h"
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

BrowserCard::BrowserCard(const QString& name, const std::filesystem::path& meshSource, QWidget* parent)
	: QFrame(parent)
	, meshName(name) {
	if (meshSource.extension() == ".obj") {
		object.emplace(*Mesh::fromObj(meshSource));
	} else {
		std::cerr << "Unsupported mesh format: " << meshSource.extension() << '\n';
		return;
	}

	if (!object) {
		std::cerr << "Failed to load mesh: " << meshSource << '\n';
		return;
	}

	setupUI();
}

BrowserCard::~BrowserCard() {}

void BrowserCard::setupUI() {
	setObjectName("BrowserCard");
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

	setMinimumSize(260, 300);

	auto* mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(12, 12, 12, 12);
	mainLayout->setSpacing(10);

	const auto mesh = object->translated(-object->baryCenter()).filledHoles().withConsistentOrientations();
	// .triangulated();
	// .subdivided();
	object.emplace(mesh);

	const auto mesh2 = object->rotated(object->principleAxes());
	object.emplace(mesh2);

	previewHost_ = new QWidget(this);
	previewHost_->setMinimumHeight(180);
	previewLayout_ = new QVBoxLayout(previewHost_);
	previewLayout_->setContentsMargins(0, 0, 0, 0);

	activatePreviewButton_ = new QPushButton("Click to load preview", previewHost_);
	activatePreviewButton_->setMinimumHeight(180);
	previewLayout_->addWidget(activatePreviewButton_);
	mainLayout->addWidget(previewHost_, 1);

	connect(activatePreviewButton_, &QPushButton::clicked, this, [this] { emit previewActivationRequested(this); });

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

void BrowserCard::setPreviewActive(bool active) {
	if (active && !preview_) {
		delete activatePreviewButton_;
		activatePreviewButton_ = nullptr;
		preview_ = new MeshPreview(object->vertexPositionBuffer(), object->triangleIndices(), previewHost_);
		preview_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
		preview_->setMinimumHeight(180);
		previewLayout_->addWidget(preview_);
	} else if (!active && preview_) {
		previewLayout_->removeWidget(preview_);

		delete preview_;
		preview_ = nullptr;

		activatePreviewButton_ = new QPushButton("Click to load preview", previewHost_);
		activatePreviewButton_->setMinimumHeight(180);
		previewLayout_->addWidget(activatePreviewButton_);

		connect(activatePreviewButton_, &QPushButton::clicked, this, [this] { emit previewActivationRequested(this); });
	}
}