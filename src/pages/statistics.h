#ifndef MULTIMEDIARETRIEVAL_STATISTICS_H
#define MULTIMEDIARETRIEVAL_STATISTICS_H

#include "../components/histogram.h"
#include "../stats/stats_table.hpp"
#include <QString>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class ShapeDb;
class QComboBox;
class QLineEdit;
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

/// @brief Computes statistics of the database after a pipeline of operations ("runs"), shows them as histograms and compares runs
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
	enum class CompareMode : std::uint8_t
	{
		Overlay,
		Difference,
	};

	void setupUI();
	void setBusy(bool busy);
	void addRun(stats::StatsTable table);
	void removeRun();
	void refreshRunLists();
	void updateClassFilter();
	void updateColumnList();
	void updateView();
	void onBucketClicked(const QVector<Sample>& samples);

	[[nodiscard]] const stats::StatsTable* selectedRun() const;
	[[nodiscard]] const stats::StatsTable* comparisonRun() const;
	[[nodiscard]] QVector<Sample> samples(const stats::StatsTable& run, size_t column, const std::string& classFilter) const;

	std::shared_ptr<ShapeDb> m_database;
	std::vector<stats::StatsTable> m_runs;
	std::optional<Histogram> m_primary;
	std::optional<Histogram> m_comparison;

	QThread* m_worker = nullptr;
	std::shared_ptr<std::atomic<bool>> m_cancel;

	QSplitter* m_splitter = nullptr;
	QLineEdit* m_pipelineEdit = nullptr;
	QComboBox* m_operationBox = nullptr;
	QListWidget* m_runList = nullptr;
	QPushButton* m_removeRunButton = nullptr;
	QListWidget* m_list = nullptr;
	QComboBox* m_compareBox = nullptr;
	QComboBox* m_modeBox = nullptr;
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
	QCheckBox* m_sameShapesCheck = nullptr;
	QComboBox* m_classFilter = nullptr;
};

#endif // MULTIMEDIARETRIEVAL_STATISTICS_H
