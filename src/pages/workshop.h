//
// Created by dafitius on 18-09-2026.
//

#ifndef MR_WORKSHOP_H
#define MR_WORKSHOP_H

#include <QWidget>

class MeshViewport;
class QLabel;
class QPushButton;

class Workshop : public QWidget
{
	Q_OBJECT

public:
	explicit Workshop(QWidget* parent = nullptr);
	~Workshop() override;
	void loadMesh(const class Mesh& mesh) const;

private:
	void setupUI();

	MeshViewport* viewport;
	QLabel* statsLabel;
	QPushButton* fixNormalsButton;
	QPushButton* fillHolesButton;
	QPushButton* normalizeTranslationButton;
	QPushButton* alignAxesButton;
	QPushButton* scaleToUnitButton;
};

#endif // MR_WORKSHOP_H
