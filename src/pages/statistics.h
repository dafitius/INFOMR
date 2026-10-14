#ifndef MULTIMEDIARETRIEVAL_STATISTICS_H
#define MULTIMEDIARETRIEVAL_STATISTICS_H

#include <QString>
#include <atomic>
#include <filesystem>
#include <memory>
#include <vector>
#include "../components/histogram.h"
#include "../stats/shape_stats.hpp"

class ShapeDb;
class QComboBox;
class QListWidget;
class QListWidgetItem;
class QSlider;
class QLabel;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QCheckBox;
class QSplitter;
class QThread;

class Statistics : public QWidget
{
	Q_OBJECT

public:
	explicit Statistics(std::shared_ptr<ShapeDb> database, QWidget* parent = nullptr);
	~Statistics() override;

signals:
	void openModel(const std::filesystem::path& source);

public slots:
	void compute();
	void loadCsv();
	void exportCsv();

private:
	void setupUI();
	void setBusy(bool busy);
	void setShapes(std::vector<stats::ShapeStats> shapes);
	void updateClassFilter();
	void rebuildHistograms();
	void onSelectionChanged();
	void onBucketClicked(const QVector<Sample>& samples);

	std::shared_ptr<ShapeDb> m_database;
	std::vector<stats::ShapeStats> m_shapes;
	QVector<Histogram> m_histograms;

	QThread* m_worker = nullptr;
	std::shared_ptr<std::atomic<bool>> m_cancel;

	QSplitter* m_splitter = nullptr;
	QListWidget* m_list = nullptr;
	HistogramView* m_view = nullptr;
	QSlider* m_bucketSlider = nullptr;
	QLabel* m_bucketLabel = nullptr;
	QLabel* m_summaryLabel = nullptr;
	QLabel* m_statusLabel = nullptr;
	QProgressBar* m_progress = nullptr;
	QPushButton* m_computeButton = nullptr;
	QPushButton* m_loadButton = nullptr;
	QPushButton* m_exportButton = nullptr;
	QCheckBox* m_limitCheck = nullptr;
	QSpinBox* m_limitSpin = nullptr;
	QComboBox* m_classFilter = nullptr;
};

#endif // MULTIMEDIARETRIEVAL_STATISTICS_H
