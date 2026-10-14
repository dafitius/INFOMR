//
// Created by dafitius on 12-10-2026.
//

#ifndef MULTIMEDIARETRIEVAL_HISTOGRAM_H
#define MULTIMEDIARETRIEVAL_HISTOGRAM_H
#include <QVector>
#include <QWidget>
#include <filesystem>

struct Sample
{
	double value = 0.0;
	std::filesystem::path source;

	Sample(const double value, const std::filesystem::path& source)
		: value(value)
		, source(source){}
};

struct Histogram
{
	QString name;
	size_t bucket_size;
	QVector<Sample> samples;

	Histogram(const QString& name, const size_t bucketSize, const QVector<Sample>& samples)
		: name(name)
		, bucket_size(bucketSize)
		, samples(samples){}
};

class HistogramView : public QWidget
{
	Q_OBJECT

public:
	explicit HistogramView(QWidget* parent = nullptr);

	void setHistogram(const Histogram* histogram);
	void setBucketCount(int count);
	size_t bucketCount() const { return m_bucketCount; }
	double bucketWidth() const;

	signals:
		void bucketClicked(const QVector<Sample>& samples);

protected:
	void paintEvent(QPaintEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;
	void leaveEvent(QEvent* event) override;

private:
	void rebuild();
	QRectF plotRect() const;
	std::optional<size_t> bucketAt(const QPoint& pos) const;

	const Histogram* m_histogram = nullptr;
	size_t m_bucketCount = 20;
	double m_min = 0.0, m_max = 1.0;
	QVector<QVector<Sample>> m_buckets;
	std::optional<size_t> m_hover_index = std::nullopt;
};

#endif // MULTIMEDIARETRIEVAL_HISTOGRAM_H
