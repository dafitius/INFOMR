#include "resultcard.h"

#include "meshpreview.h"

#include <QLabel>
#include <QVBoxLayout>
#include <iostream>

ResultCard::ResultCard(const QString& name, const std::filesystem::path& meshSource, const QString& subtitle, QWidget* parent)
	: QFrame(parent) {
	if (const auto loaded = Mesh::fromObj(meshSource))
		mesh_.emplace(*loaded);
	else
		std::cerr << "Failed to load mesh preview: " << meshSource.string() << '\n';

	setupUI(name, subtitle);
}

ResultCard::ResultCard(const QString& name, const Mesh& mesh, const QString& subtitle, QWidget* parent)
	: QFrame(parent)
	, mesh_(mesh) {
	setupUI(name, subtitle);
}

void ResultCard::setupUI(const QString& name, const QString& subtitle) {
	setObjectName("resultCard");
	setFrameShape(QFrame::StyledPanel);
	setMinimumWidth(250);
	setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

	auto* layout = new QVBoxLayout(this);
	layout->setContentsMargins(10, 10, 10, 10);
	layout->setSpacing(8);

	auto* title = new QLabel(name, this);
	title->setWordWrap(true);
	title->setObjectName("cardTitle");
	layout->addWidget(title);

	if (mesh_) {
		auto* preview = new MeshPreview(mesh_->vertexPositionBuffer(), mesh_->triangleIndices(), this);
		preview->setMinimumHeight(180);
		layout->addWidget(preview, 1);

		auto* stats = new QLabel(QString("Vertices: %1 | Faces: %2").arg(mesh_->numVertices()).arg(mesh_->numFaces()), this);
		stats->setObjectName("cardSubtitle");
		layout->addWidget(stats);
	} else {
		auto* error = new QLabel("Preview unavailable", this);
		error->setMinimumHeight(180);
		error->setAlignment(Qt::AlignCenter);
		error->setObjectName("cardSubtitle");
		layout->addWidget(error, 1);
	}

	auto* info = new QLabel(subtitle, this);
	info->setWordWrap(true);
	info->setObjectName("cardSubtitle");
	layout->addWidget(info);
}
