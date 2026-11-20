//
// Created by dafitius on 12-10-2026.
//

#ifndef MULTIMEDIARETRIEVAL_HISTOGRAM_H
#define MULTIMEDIARETRIEVAL_HISTOGRAM_H
#include <QVector>
#include <QWidget>
#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <utility>
#include <vector>

struct Sample {
	double value = 0.0;
	std::filesystem::path source;

	Sample(const double value, const std::filesystem::path& source)
		: value(value)
		, source(source) {}
};

struct Histogram {
	/// @brief Aggregates over the finite sample values
	struct Summary {
		size_t count = 0;
		double min = 0.0;
		double max = 0.0;
		double mean = 0.0;
		double median = 0.0;
		double stddev = 0.0;
	};

	QString name;
	size_t bucket_size; // default number of buckets to show
	QVector<Sample> samples;

	Histogram(const QString& name, const size_t bucketSize, const QVector<Sample>& samples)
		: name(name)
		, bucket_size(bucketSize)
		, samples(samples) {}

	/// @brief A bucket count that suits the number of samples (square root rule, clamped to [1, maxBuckets])
	static size_t defaultBucketCount(size_t numSamples, size_t maxBuckets = 200);

	[[nodiscard]] Summary summary() const;
};

class HistogramView : public QWidget {
	Q_OBJECT

public:
	enum class Scale : std::uint8_t {
		Linear,
		Log,
	};

	explicit HistogramView(QWidget* parent = nullptr);

	void setHistogram(const Histogram* histogram);

	void setHistograms(const Histogram* primary, const Histogram* comparison);

	void setBucketCount(int count);

	void setScale(Scale scale);

	void setClipOutliers(bool clip);

	void setFixedRange(std::optional<std::pair<double, double>> range);

	size_t bucketCount() const {
		return m_bucketCount;
	}

signals:
	void bucketClicked(const QVector<Sample>& samples);

protected:
	void paintEvent(QPaintEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;
	void leaveEvent(QEvent* event) override;

private:
	enum class SlotKind : std::uint8_t {
		Below,
		Bucket,
		Above,
	};

	struct BarRef {
		size_t slot;
		size_t series; // 0 for the primary histogram, 1 for the comparison

		bool operator==(const BarRef&) const = default;
	};

	void rebuild();

	std::vector<double> axisPositions();
	QRectF plotRect() const;
	std::optional<BarRef> barAt(const QPoint& pos) const;
	[[nodiscard]] size_t numSeries() const;
	[[nodiscard]] size_t numSlots() const;
	[[nodiscard]] SlotKind slotKind(size_t slot) const;
	[[nodiscard]] size_t firstBucketSlot() const;

	[[nodiscard]] size_t slotOf(double value) const;

	[[nodiscard]] double toAxis(double value) const;
	[[nodiscard]] double fromAxis(double position) const;

	[[nodiscard]] double bucketStart(size_t bucket) const;
	[[nodiscard]] QString slotText(size_t slot) const;

	std::array<const Histogram*, 2> m_histograms{};
	size_t m_bucketCount = 20;
	Scale m_scale = Scale::Linear;
	bool m_clipOutliers = false;
	std::optional<std::pair<double, double>> m_fixedRange;

	size_t m_numBuckets = 0;
	double m_low = 0.0;
	double m_width = 1.0;
	double m_clipLow = 0.0;	 // values below go in the below slot
	double m_clipHigh = 0.0; // values above go in the above slot
	double m_logUnit = 1.0;	 // values much smaller than this are spaced linearly on the log scale
	bool m_wholeNumbers = false;
	bool m_hasBelow = false;
	bool m_hasAbove = false;

	std::array<QVector<QVector<Sample>>, 2> m_buckets;
	std::optional<BarRef> m_hover = std::nullopt;
};

#endif // MULTIMEDIARETRIEVAL_HISTOGRAM_H
