//
// Created by dafitius on 18-09-2026.
//

#ifndef MR_WORKSHOP_H
#define MR_WORKSHOP_H

#include "../components/histogram.h"

#include <QWidget>
#include <optional>

class MeshViewport;
class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;

class Workshop : public QWidget
{
	Q_OBJECT

public:
	explicit Workshop(QWidget* parent = nullptr);
	~Workshop() override;
	void loadMesh(const class Mesh& mesh);
	void recalcStats();

private:
	void setupUI();
	QWidget* setupDistributionPanel(QWidget* parent);

	void updateDistribution();

	MeshViewport* viewport;
	QLabel* statsLabel;
	QComboBox* distributionBox;
	QSpinBox* distributionBinsSpin;
	HistogramView* distributionView;
	QLabel* distributionSummary;
	std::optional<Histogram> distributionHistogram;
	QPushButton* runPipelineButton;
	QPushButton* fixNormalsButton;
	QPushButton* triangulateButton;
	QPushButton* removeNonManifoldButton;
	QPushButton* fillHolesButton;
	QPushButton* subdivideButton;
	QPushButton* normalizeTranslationButton;
	QPushButton* alignAxesButton;
	QPushButton* normalizePoseButton;
	QPushButton* scaleToUnitButton;
};

#endif // MR_WORKSHOP_H
