//
// Created by dafitius on 12-10-2026.
//

#include "histogram.h"

#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>
#include <iostream>

HistogramView::HistogramView(QWidget* parent) : QWidget(parent)
{
	setMouseTracking(true);
	setMinimumSize(300, 200);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void HistogramView::setHistogram(const Histogram* histogram)
{
	m_histogram = histogram;
	rebuild();
}

void HistogramView::setBucketCount(const int count)
{
	m_bucketCount = std::max(1, count);
	rebuild();
}

double HistogramView::bucketWidth() const
{
	return (m_max - m_min) / static_cast<double>(m_bucketCount);
}

void HistogramView::rebuild()
{
	m_buckets.clear();
	m_hover_index = std::nullopt;

	if ((m_histogram != nullptr) && !m_histogram->samples.isEmpty())
	{
		const auto& s = m_histogram->samples;
		auto [low, high] = std::minmax_element(s.begin(), s.end(),
			[](const Sample& a, const Sample& b) { return a.value < b.value; });
		m_min = low->value;
		m_max = high->value;
		if (m_max <= m_min) m_max = m_min + 1.0;

		m_buckets.resize(m_bucketCount);
		const double w = bucketWidth();
		for (const Sample& sample : s)
		{
			auto const index = std::min(static_cast<size_t>((sample.value - m_min) / w), m_bucketCount - 1);
			m_buckets[index].append(sample);
		}
	}
	update();
}

QRectF HistogramView::plotRect() const
{
	return QRectF(rect()).adjusted(50, 15, -15, -35);
}

std::optional<size_t> HistogramView::bucketAt(const QPoint& pos) const
{
	if (m_buckets.isEmpty()) return std::nullopt;
	const auto rect = plotRect();
	if (!rect.contains(pos)) return std::nullopt;
	const auto bucketIndex = (pos.x() - rect.left()) / rect.width() * static_cast<double>(m_bucketCount);
	return std::clamp(static_cast<size_t>(bucketIndex), static_cast<size_t>(0), m_bucketCount - 1);
}

void HistogramView::paintEvent(QPaintEvent*)
{
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);
	p.fillRect(rect(), palette().base());

	if (m_buckets.isEmpty())
	{
		p.setPen(palette().text().color());
		p.drawText(rect(), Qt::AlignCenter, tr("No histograms to show. press Load to add them."));
		return;
	}

	const auto rect = plotRect();
	size_t const maxCount = std::ranges::max(m_buckets, {}, [](const auto& bucket)
	{
		return static_cast<size_t>(bucket.size());
	}).size();

	// Axes
	p.setPen(palette().text().color());
	p.drawLine(rect.bottomLeft(), rect.bottomRight());
	p.drawLine(rect.bottomLeft(), rect.topLeft());
	p.drawText(QRectF(0, rect.top() - 8, 45, 16), Qt::AlignRight | Qt::AlignVCenter, QString::number(maxCount));
	p.drawText(QRectF(0, rect.bottom() - 8, 45, 16), Qt::AlignRight | Qt::AlignVCenter, "0");
	p.drawText(QRectF(rect.left() - 30, rect.bottom() + 4, 60, 18), Qt::AlignCenter, QString::number(m_min, 'g', 5));
	p.drawText(QRectF(rect.right() - 30, rect.bottom() + 4, 60, 18), Qt::AlignCenter, QString::number(m_max, 'g', 5));

	// Bars
	const double bucketWidth = rect.width() / static_cast<double>(m_bucketCount);
	for (const auto& [i, bucket] : m_buckets | std::views::enumerate)
	{
		double const height = rect.height() * bucket.size() / static_cast<double>(maxCount);
		auto const bar = QRectF(rect.left() + i * bucketWidth, rect.bottom() - height, bucketWidth, height);
		p.setPen(palette().base().color());
		p.setBrush(i == m_hover_index ? palette().highlight() : QBrush(QColor(70, 130, 200)));
		p.drawRect(bar);
	}
}

void HistogramView::mouseMoveEvent(QMouseEvent* event)
{
	auto const i = bucketAt(event->position().toPoint());
	if (i != m_hover_index)
	{
		m_hover_index = i;
		update();
	}

	if (!i.has_value()) return;
	const auto index = static_cast<qsizetype>(*i);
	const double width = bucketWidth();
	const double low = m_min + (*i * width);
	const double high = low + width;

	QToolTip::showText(event->globalPosition().toPoint(),
	tr("bucket %1 (%2 -> %3)\n%4 models\nClick to open a random one")
	.arg(index)
	.arg(low, 0, 'g', 5)
	.arg(high, 0, 'g', 5)
	.arg(m_buckets[index].size()), this);
	setCursor(m_buckets[index].isEmpty() ? Qt::ArrowCursor : Qt::PointingHandCursor);
}

void HistogramView::mousePressEvent(QMouseEvent* event)
{
	if (event->button() != Qt::LeftButton) return;
	auto const i = bucketAt(event->position().toPoint());
	if (!i.has_value()) return;
	const auto index = static_cast<qsizetype>(*i);
	if (m_buckets[index].isEmpty()) return;
	emit bucketClicked(m_buckets[index]);
}

void HistogramView::leaveEvent(QEvent*)
{
	m_hover_index = std::nullopt;
	update();
}
