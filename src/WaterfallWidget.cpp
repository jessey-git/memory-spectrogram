/* SPDX-FileCopyrightText: 2026 Jesse Yurkovich
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "WaterfallWidget.h"

#include <QPainter>

#include <algorithm>
#include <array>
#include <vector>

struct pair_hash {
  template<class T1, class T2> std::size_t operator()(const std::pair<T1, T2> &p) const
  {
    const std::size_t h1 = std::hash<T1>{}(p.first);
    const std::size_t h2 = std::hash<T2>{}(p.second);
    return h1 ^ (h2 << 1);
  }
};

struct pair_equal {
  template<class T1, class T2>
  bool operator()(const std::pair<T1, T2> &a, const std::pair<T1, T2> &b) const
  {
    return a.first == b.first && a.second == b.second;
  }
};

constexpr double MAX_TIME_WINDOW_MS = 30000.0;

// Viridis colormap
constexpr int NUM_COLORS = 400;
constexpr std::array<QColor, NUM_COLORS + 1> COLOR_MAP = []() {
  std::array<QColor, NUM_COLORS + 1> map;
  for (int i = 0; i <= NUM_COLORS; ++i) {
    const double t = i / double(NUM_COLORS);

    double r = 0;
    double g = 0;
    double b = 0;

    if (t < 0.5) {
      r = 0.267004 + 2 * t * (0.127568 - 0.267004);
      g = 0.004874 + 2 * t * (0.566949 - 0.004874);
      b = 0.329415 + 2 * t * (0.550556 - 0.329415);
    }
    else {
      const double t2 = 2 * (t - 0.5);
      r = 0.127568 + t2 * (0.993248 - 0.127568);
      g = 0.566949 + t2 * (0.906157 - 0.566949);
      b = 0.550556 + t2 * (0.143936 - 0.550556);
    }

    map[i] = QColor(int(r * 255), int(g * 255), int(b * 255));
  }
  return map;
}();

constexpr std::array<size_t, 36> SIZE_BUCKETS = {
    8,     16,    32,    48,    64,    80,    96,    112,    128,    160,    192,     224,
    256,   320,   384,   448,   512,   640,   768,   896,    1024,   2048,   4096,    8192,
    16384, 24576, 32768, 49152, 65536, 81920, 98304, 114688, 131072, 524288, 2097152, ULLONG_MAX};

WaterfallWidget::WaterfallWidget(QWidget *parent) : QWidget(parent)
{
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void WaterfallWidget::setData(const AllocationEvents &events)
{
  events_ = events;
  liveMode_ = false;
  currentTimeMs_ = 0.0;
  updateVisualization();
}

void WaterfallWidget::setLiveMode(bool enabled)
{
  liveMode_ = enabled;
  if (!enabled) {
    currentTimeMs_ = 0.0;
  }
}

QSize WaterfallWidget::sizeHint() const
{
  return QSize(800, 600);
}

QColor WaterfallWidget::getColorForCount(int count) const
{
  if (count < 0)
    count = 0;
  if (count > NUM_COLORS)
    count = NUM_COLORS;
  return COLOR_MAP[count];
}

int WaterfallWidget::getSizeBucketIndex(size_t size)
{
  const auto index = std::lower_bound(SIZE_BUCKETS.begin(), SIZE_BUCKETS.end(), size);
  return int(std::distance(SIZE_BUCKETS.begin(), index));
}

void WaterfallWidget::processDataForCurrentSize()
{
  if (width() <= 0) {
    return;
  }

  if (events_.empty() && !liveMode_) {
    return;
  }

  double displayTimeRange = MAX_TIME_WINDOW_MS;
  double endTime;
  double startTime;

  if (liveMode_) {
    endTime = currentTimeMs_;
    startTime = endTime - displayTimeRange;
    if (startTime < 0) {
      startTime = 0;
    }
  }
  else {
    if (events_.empty()) {
      return;
    }

    double minTime = events_[0].timeMs;
    double maxTime = events_[0].timeMs;

    for (const AllocationEvent &event : events_) {
      minTime = std::min(minTime, event.timeMs);
      maxTime = std::max(maxTime, event.timeMs);
    }

    double timeRange = maxTime - minTime;
    displayTimeRange = std::min(timeRange, MAX_TIME_WINDOW_MS);
    startTime = minTime;
    endTime = startTime + displayTimeRange;
  }

  const double timeBucketMs = displayTimeRange / width();

  data_.prepare(width(), SIZE_BUCKETS.size());

  for (const AllocationEvent &event : events_) {
    if (event.timeMs < startTime || event.timeMs > endTime) {
      continue;
    }

    int timeBucket = int((event.timeMs - startTime) / timeBucketMs);
    if (timeBucket >= width()) {
      timeBucket = width() - 1;
    }
    const int sizeBucket = WaterfallWidget::getSizeBucketIndex(event.size);

    data_.incrementCount(timeBucket, sizeBucket);
  }
}

void WaterfallWidget::updateVisualization()
{
  if (width() <= 0 || height() <= 0) {
    return;
  }

  const int pixmapHeight = height();
  const int bucketHeight = std::max(1, pixmapHeight / int(SIZE_BUCKETS.size()));

  pixmap_.fill(Qt::black);

  processDataForCurrentSize();

  QPainter painter(&pixmap_);
  data_.process([&](int x, int y, int count) {
    y = pixmapHeight - (y + 1) * bucketHeight;
    const QColor color = getColorForCount(count);
    painter.fillRect(x, y, 1, bucketHeight, color);
  });

  update();
}

void WaterfallWidget::paintEvent(QPaintEvent *event)
{
  QPainter painter(this);

  if (!pixmap_.isNull()) {
    painter.drawPixmap(0, 0, pixmap_);
  }
  else {
    painter.fillRect(rect(), Qt::black);
    painter.setPen(Qt::white);
    painter.drawText(rect(), Qt::AlignCenter, "No data loaded");
  }
}

void WaterfallWidget::resizeEvent(QResizeEvent *event)
{
  QWidget::resizeEvent(event);

  pixmap_ = QPixmap(width(), height());
  updateVisualization();
}
