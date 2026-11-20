//
// Created by dafitius on 12-10-2026.
//

#include "histogram.h"

#include <QEvent>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QSizePolicy>
#include <QString>
#include <QToolTip>
#include <QWidget>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <ranges>
#include <utility>
#include <vector>

size_t Histogram::defaultBucketCount(const size_t numSamples, const size_t maxBuckets)
{
	const auto buckets = static_cast<size_t>(std::ceil(std::sqrt(static_cast<double>(numSamples))));
	return std::clamp(buckets, size_t{1}, maxBuckets);
}

Histogram::Summary Histogram::summary() const
{
	std::vector<double> values;
	values.reserve(samples.size());
	for (const Sample& sample : samples)
	{
		if (std::isfinite(sample.value))
		{
			values.push_back(sample.value);
		}
	}

	Summary result;
	if (values.empty())
	{
		return result;
	}

	std::ranges::sort(values);
	result.count = values.size();
	result.min = values.front();
	result.max = values.back();

	const size_t mid = values.size() / 2;
	result.median = values.size() % 2 == 0 ? (values[mid - 1] + values[mid]) / 2.0 : values[mid];

	double sum = 0.0;
	for (const double value : values)
	{
		sum += value;
	}
	result.mean = sum / static_cast<double>(values.size());

	double squaredError = 0.0;
	for (const double value : values)
	{
		squaredError += (value - result.mean) * (value - result.mean);
	}
	result.stddev = std::sqrt(squaredError / static_cast<double>(values.size()));
	return result;
}

namespace
{
	const std::array seriesColors{QColor(70, 130, 200), QColor(230, 140, 60)};

	constexpr double clipFraction = 0.01;	 // clipping keeps the 1st to 99th percentile
	constexpr double minLabelSpacing = 70.0; // pixels between x axis labels
	constexpr double logUnitFraction = 0.05;

	double percentile(const std::vector<double>& sorted, const double p)
	{
		const auto index = static_cast<size_t>(p * static_cast<double>(sorted.size() - 1));
		return sorted[index];
	}
} // namespace

HistogramView::HistogramView(QWidget* parent)
	: QWidget(parent)
{
	setMouseTracking(true);
	setMinimumSize(300, 200);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void HistogramView::setHistogram(const Histogram* histogram)
{
	setHistograms(histogram, nullptr);
}

void HistogramView::setHistograms(const Histogram* primary, const Histogram* comparison)
{
	m_histograms = {primary, comparison};
	rebuild();
}

void HistogramView::setBucketCount(const int count)
{
	m_bucketCount = static_cast<size_t>(std::max(1, count));
	rebuild();
}

void HistogramView::setScale(const Scale scale)
{
	m_scale = scale;
	rebuild();
}

void HistogramView::setClipOutliers(const bool clip)
{
	m_clipOutliers = clip;
	rebuild();
}

void HistogramView::setFixedRange(std::optional<std::pair<double, double>> range)
{
	m_fixedRange = range;
	rebuild();
}

size_t HistogramView::numSeries() const
{
	return m_histograms[1] != nullptr ? 2 : 1;
}

size_t HistogramView::numSlots() const
{
	return m_numBuckets + (m_hasBelow ? 1 : 0) + (m_hasAbove ? 1 : 0);
}

size_t HistogramView::firstBucketSlot() const
{
	return m_hasBelow ? 1 : 0;
}

HistogramView::SlotKind HistogramView::slotKind(const size_t slot) const
{
	if (m_hasBelow && slot == 0)
	{
		return SlotKind::Below;
	}
	if (m_hasAbove && slot == numSlots() - 1)
	{
		return SlotKind::Above;
	}
	return SlotKind::Bucket;
}

double HistogramView::toAxis(const double value) const
{
	if (m_scale == Scale::Linear)
	{
		return value;
	}
	return std::copysign(std::log10(1.0 + (std::abs(value) / m_logUnit)), value);
}

double HistogramView::fromAxis(const double position) const
{
	if (m_scale == Scale::Linear)
	{
		return position;
	}
	return std::copysign((std::pow(10.0, std::abs(position)) - 1.0) * m_logUnit, position);
}

double HistogramView::bucketStart(const size_t bucket) const
{
	const double start = m_low + (static_cast<double>(bucket) * m_width);

	return m_wholeNumbers ? start + 0.5 : fromAxis(start);
}

QString HistogramView::slotText(const size_t slot) const
{
	switch (slotKind(slot))
	{
		case SlotKind::Below: return tr("below %1").arg(fromAxis(m_clipLow), 0, 'g', 5);
		case SlotKind::Above: return tr("above %1").arg(fromAxis(m_clipHigh), 0, 'g', 5);
		case SlotKind::Bucket: break;
	}

	const auto bucket = slot - firstBucketSlot();
	if (m_wholeNumbers)
	{
		const double first = bucketStart(bucket);
		const double last = first + m_width - 1.0;
		return m_width <= 1.0 ? QString::number(first, 'f', 0) : tr("%1 to %2").arg(first, 0, 'f', 0).arg(last, 0, 'f', 0);
	}
	return tr("%1 to %2").arg(bucketStart(bucket), 0, 'g', 5).arg(bucketStart(bucket + 1), 0, 'g', 5);
}

size_t HistogramView::slotOf(const double value) const
{
	const double position = toAxis(value);
	if (position < m_clipLow)
	{
		return 0;
	}
	if (position > m_clipHigh)
	{
		return numSlots() - 1;
	}
	const auto bucket = std::min(static_cast<size_t>((position - m_low) / m_width), m_numBuckets - 1);
	return firstBucketSlot() + bucket;
}

std::vector<double> HistogramView::axisPositions()
{
	std::vector<double> values;
	for (size_t series = 0; series < numSeries(); ++series)
	{
		for (const Sample& sample : m_histograms[series]->samples | std::views::filter([](const Sample& s) { return std::isfinite(s.value); }))
		{
			values.push_back(sample.value);
		}
	}
	const bool allWholeNumbers = std::ranges::all_of(values, [](const double value) { return value == std::round(value); });
	m_wholeNumbers = allWholeNumbers && m_scale == Scale::Linear;

	std::vector<double> magnitudes;
	for (const double value : values | std::views::filter([](const double value) { return value != 0.0; }))
	{
		magnitudes.push_back(std::abs(value));
	}
	std::ranges::sort(magnitudes);
	m_logUnit = allWholeNumbers || magnitudes.empty() ? 1.0 : percentile(magnitudes, logUnitFraction);

	std::ranges::transform(values, values.begin(), [this](const double value) { return toAxis(value); });
	std::ranges::sort(values);
	return values;
}

void HistogramView::rebuild()
{
	for (auto& buckets : m_buckets)
	{
		buckets.clear();
	}
	m_hover = std::nullopt;
	m_numBuckets = 0;
	m_hasBelow = false;
	m_hasAbove = false;

	if (m_histograms[0] == nullptr)
	{
		update();
		return;
	}

	const auto positions = axisPositions();
	if (positions.empty())
	{
		update();
		return;
	}

	if (m_fixedRange)
	{
		m_clipLow = toAxis(m_fixedRange->first);
		m_clipHigh = toAxis(m_fixedRange->second);
	}
	else
	{
		m_clipLow = m_clipOutliers ? percentile(positions, clipFraction) : positions.front();
		m_clipHigh = m_clipOutliers ? percentile(positions, 1.0 - clipFraction) : positions.back();
	}

	if (m_wholeNumbers)
	{
		// Every bucket spans the same whole number of values, so e.g. 0 and 1 never share a bucket when there is room for both
		m_clipLow = std::floor(m_clipLow);
		m_clipHigh = std::ceil(m_clipHigh);
		const double numValues = m_clipHigh - m_clipLow + 1.0;
		m_width = std::max(1.0, std::ceil(numValues / static_cast<double>(m_bucketCount)));
		m_numBuckets = static_cast<size_t>(std::ceil(numValues / m_width));
		m_low = m_clipLow - 0.5;
	}
	else
	{
		if (m_clipHigh <= m_clipLow)
		{
			m_clipHigh = m_clipLow + 1.0;
		}
		m_numBuckets = m_bucketCount;
		m_width = (m_clipHigh - m_clipLow) / static_cast<double>(m_numBuckets);
		m_low = m_clipLow;
	}

	m_hasBelow = positions.front() < m_clipLow;
	m_hasAbove = positions.back() > m_clipHigh;

	for (size_t series = 0; series < numSeries(); ++series)
	{
		auto& seriesSlots = m_buckets[series];
		seriesSlots.resize(static_cast<qsizetype>(numSlots()));
		for (const Sample& sample : m_histograms[series]->samples)
		{
			if (std::isfinite(sample.value))
			{
				seriesSlots[static_cast<qsizetype>(slotOf(sample.value))].append(sample);
			}
		}
	}
	update();
}

QRectF HistogramView::plotRect() const
{
	return QRectF(rect()).adjusted(50, 15, -15, -35);
}

std::optional<HistogramView::BarRef> HistogramView::barAt(const QPoint& pos) const
{
	if (m_buckets[0].isEmpty())
	{
		return std::nullopt;
	}
	const auto rect = plotRect();
	if (!rect.contains(pos))
	{
		return std::nullopt;
	}

	const auto numBars = numSlots() * numSeries();
	const auto bar = (pos.x() - rect.left()) / rect.width() * static_cast<double>(numBars);
	const auto barIndex = std::clamp(static_cast<size_t>(bar), size_t{0}, numBars - 1);
	return BarRef{.slot = barIndex / numSeries(), .series = barIndex % numSeries()};
}

void HistogramView::paintEvent(QPaintEvent*)
{
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.fillRect(rect(), palette().base());

	if (m_buckets[0].isEmpty())
	{
		painter.setPen(palette().text().color());
		painter.drawText(rect(), Qt::AlignCenter, tr("No histogram to show. Compute or load statistics first."));
		return;
	}

	const auto rect = plotRect();
	qsizetype maxCount = 1;
	for (size_t series = 0; series < numSeries(); ++series)
	{
		for (const auto& slot : m_buckets[series])
		{
			maxCount = std::max(maxCount, slot.size());
		}
	}

	// Axes
	const double slotWidth = rect.width() / static_cast<double>(numSlots());
	painter.setPen(palette().text().color());
	painter.drawLine(rect.bottomLeft(), rect.bottomRight());
	painter.drawLine(rect.bottomLeft(), rect.topLeft());
	painter.drawText(QRectF(0, rect.top() - 8, 45, 16), Qt::AlignRight | Qt::AlignVCenter, QString::number(maxCount));
	painter.drawText(QRectF(0, rect.bottom() - 8, 45, 16), Qt::AlignRight | Qt::AlignVCenter, "0");

	const auto labelStep = std::max(size_t{1}, static_cast<size_t>(std::ceil(minLabelSpacing / slotWidth)));
	const auto lastLabel = m_wholeNumbers ? m_numBuckets - 1 : m_numBuckets;
	for (size_t bucket = 0; bucket <= lastLabel; bucket += labelStep)
	{
		const double edge = rect.left() + (static_cast<double>(firstBucketSlot() + bucket) * slotWidth);
		const double labelX = m_wholeNumbers ? edge + (slotWidth * 0.5) : edge;
		painter.drawLine(QPointF(labelX, rect.bottom()), QPointF(labelX, rect.bottom() + 3));
		painter.drawText(QRectF(labelX - 35, rect.bottom() + 4, 70, 18), Qt::AlignCenter, QString::number(bucketStart(bucket), 'g', 4));
	}
	if (m_hasBelow)
	{
		painter.drawText(QRectF(rect.left(), rect.bottom() + 18, slotWidth, 16), Qt::AlignCenter, tr("<"));
	}
	if (m_hasAbove)
	{
		painter.drawText(QRectF(rect.right() - slotWidth, rect.bottom() + 18, slotWidth, 16), Qt::AlignCenter, tr(">"));
	}

	const double barWidth = slotWidth / static_cast<double>(numSeries());
	for (size_t series = 0; series < numSeries(); ++series)
	{
		for (const auto& [i, slot] : m_buckets[series] | std::views::enumerate)
		{
			const auto index = static_cast<size_t>(i);
			const bool isBucket = slotKind(index) == SlotKind::Bucket;
			double const height = rect.height() * static_cast<double>(slot.size()) / static_cast<double>(maxCount);
			const double left = rect.left() + (static_cast<double>((index * numSeries()) + series) * barWidth);
			auto const bar = QRectF(left, rect.bottom() - height, barWidth, height);
			const QBrush brush(seriesColors[series], isBucket ? Qt::SolidPattern : Qt::BDiagPattern);
			painter.setPen(isBucket ? palette().base().color() : seriesColors[series]);
			painter.setBrush(m_hover == BarRef{.slot = index, .series = series} ? palette().highlight() : brush);
			painter.drawRect(bar);
		}
	}

	if (numSeries() > 1)
	{
		const QFontMetrics metrics(font());
		double top = rect.top();
		for (size_t series = 0; series < numSeries(); ++series)
		{
			const QString name = m_histograms[series]->name;
			const double textWidth = metrics.horizontalAdvance(name);
			const QRectF swatch(rect.right() - textWidth - 22, top + 3, 10, 10);
			painter.setPen(Qt::NoPen);
			painter.setBrush(seriesColors[series]);
			painter.drawRect(swatch);
			painter.setPen(palette().text().color());
			painter.drawText(QRectF(swatch.right() + 6, top, textWidth + 4, 16), Qt::AlignLeft | Qt::AlignVCenter, name);
			top += 18;
		}
	}
}

void HistogramView::mouseMoveEvent(QMouseEvent* event)
{
	auto const bar = barAt(event->position().toPoint());
	if (bar != m_hover)
	{
		m_hover = bar;
		update();
	}

	if (!bar.has_value())
	{
		return;
	}
	const auto index = static_cast<qsizetype>(bar->slot);

	QString counts;
	for (size_t series = 0; series < numSeries(); ++series)
	{
		const auto count = m_buckets[series][index].size();
		counts += numSeries() > 1 ? tr("\n%1: %2 models").arg(m_histograms[series]->name).arg(count) : tr("\n%1 models").arg(count);
	}

	QToolTip::showText(event->globalPosition().toPoint(), tr("%1%2\nClick to open a random one").arg(slotText(bar->slot)).arg(counts), this);
	setCursor(m_buckets[bar->series][index].isEmpty() ? Qt::ArrowCursor : Qt::PointingHandCursor);
}

void HistogramView::mousePressEvent(QMouseEvent* event)
{
	if (event->button() != Qt::LeftButton)
	{
		return;
	}
	auto const bar = barAt(event->position().toPoint());
	if (!bar.has_value())
	{
		return;
	}
	const auto& slot = m_buckets[bar->series][static_cast<qsizetype>(bar->slot)];
	if (slot.isEmpty())
	{
		return;
	}
	emit bucketClicked(slot);
}

void HistogramView::leaveEvent(QEvent*)
{
	m_hover = std::nullopt;
	update();
}
