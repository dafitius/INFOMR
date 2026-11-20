#ifndef MESHCARD_H
#define MESHCARD_H

#include <QFrame>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>
#include <filesystem>

#include "../geometry/mesh.hpp"
#include "meshpreview.h"

class BrowserCard : public QFrame {
	Q_OBJECT

public:
	BrowserCard(const QString& name, const std::filesystem::path& meshSource, QWidget* parent = nullptr);
	~BrowserCard() override;
	void setPreviewActive(bool active);

signals:
	void meshClicked(const QString& meshName);
	void previewActivationRequested(BrowserCard* card);

private:
	void setupUI();

	QString meshName;
	std::optional<Mesh> object;
	QWidget* previewHost_ = nullptr;
	QVBoxLayout* previewLayout_ = nullptr;
	MeshPreview* preview_ = nullptr;
	QPushButton* activatePreviewButton_ = nullptr;
};

#endif // MESHCARD_H