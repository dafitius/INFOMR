#ifndef MULTIMEDIARETRIEVAL_STATISTICS_H
#define MULTIMEDIARETRIEVAL_STATISTICS_H

#include <QFutureWatcher>
#include <QString>
#include <filesystem>
#include <functional>
#include "../components/histogram.h"


class QListWidget;
class QListWidgetItem;
class QSlider;
class QLabel;
class QPushButton;
class QSpinBox;
class QCheckBox;
class QSplitter;

class Statistics : public QWidget
{
	Q_OBJECT

public:
	using Provider = std::function<QVector<Histogram>(int limit)>;

	explicit Statistics(QWidget* parent = nullptr);
	~Statistics() override;
	void setProvider(Provider provider);
signals:
	void openModel(const std::filesystem::path& source);

public slots:
	void refresh();

private:
	void setupUI();
	void onSelectionChanged();
	void onBucketClicked(const QVector<Sample>& samples);

	Provider m_provider;
	QVector<Histogram> m_histograms;

	QSplitter* m_splitter = nullptr;
	QListWidget* m_list = nullptr;
	HistogramView* m_view = nullptr;
	QSlider* m_bucketSlider = nullptr;
	QLabel* m_bucketLabel = nullptr;
	QPushButton* m_refreshButton = nullptr;
	QCheckBox* m_limitCheck = nullptr;
	QSpinBox* m_limitSpin = nullptr;
};

#endif // MULTIMEDIARETRIEVAL_STATISTICS_H