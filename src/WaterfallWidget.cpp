#include "WaterfallWidget.h"

#include <QPainter>

#include <algorithm>
#include <array>
#include <unordered_map>
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

constexpr int NUM_COLORS = 128;
constexpr std::array<size_t, 34> SIZE_BUCKETS = {
    8,    16,   32,    48,    64,    80,    96,    112,   128,   160,    192,  224,
    256,  320,  384,   448,   512,   640,   768,   896,   1024,  1792,   2688, 4032,
    5376, 8192, 16448, 24640, 32832, 41024, 49216, 57408, 65600, INT_MAX};

WaterfallWidget::WaterfallWidget(QWidget *parent)
    : QWidget(parent), liveMode_(false), currentTimeMs_(0.0)
{
  viridisColors_.resize(NUM_COLORS + 1);
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

    viridisColors_[i] = QColor(int(r * 255), int(g * 255), int(b * 255));
  }

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

void WaterfallWidget::setCurrentTime(double timeMs)
{
  currentTimeMs_ = timeMs;
  if (liveMode_) {
    updateVisualization();
  }
}

void WaterfallWidget::updateLiveData(AllocationEvents &&events)
{
  if (!liveMode_) {
    return;
  }

  events_ = std::move(events);
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
  return viridisColors_[count];
}

int WaterfallWidget::getSizeBucketIndex(size_t size)
{
  for (size_t i = 0; i < SIZE_BUCKETS.size(); ++i) {
    if (size <= SIZE_BUCKETS[i]) {
      return int(i);
    }
  }
  return int(SIZE_BUCKETS.size());
}

std::vector<BucketData> WaterfallWidget::processDataForCurrentSize()
{
  if (width() <= 0) {
    return {};
  }

  if (events_.empty() && !liveMode_) {
    return {};
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
      return {};
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

  double timeBucketMs = displayTimeRange / width();
  if (timeBucketMs < 0.1) {
    timeBucketMs = 0.1;
  }

  std::unordered_map<std::pair<int, int>, int, pair_hash, pair_equal> bucketMap;
  bucketMap.reserve(16000);

  for (const AllocationEvent &event : events_) {
    if (event.timeMs < startTime || event.timeMs > endTime) {
      continue;
    }

    int timeBucket = int((event.timeMs - startTime) / timeBucketMs);
    if (timeBucket >= width()) {
      timeBucket = width() - 1;
    }
    const int sizeBucket = WaterfallWidget::getSizeBucketIndex(event.size);

    auto key = std::make_pair(timeBucket, sizeBucket);
    bucketMap[key]++;
  }

  std::vector<BucketData> result;
  result.reserve(bucketMap.size());
  for (const auto &[key, count] : bucketMap) {
    result.push_back({key.first, key.second, count});
  }

  return result;
}

void WaterfallWidget::updateVisualization()
{
  if (width() <= 0 || height() <= 0) {
    return;
  }

  const int pixmapWidth = width();
  const int pixmapHeight = height();
  const int bucketHeight = std::max(1, pixmapHeight / int(SIZE_BUCKETS.size()));

  pixmap_ = QPixmap(pixmapWidth, pixmapHeight);
  pixmap_.fill(Qt::black);

  QPainter painter(&pixmap_);

  const std::vector<BucketData> bucketData = processDataForCurrentSize();
  for (const BucketData &bucket : bucketData) {
    if (bucket.timeBucket >= pixmapWidth || bucket.sizeBucket > SIZE_BUCKETS.size()) {
      continue;
    }

    const int x = bucket.timeBucket;
    const int y = pixmapHeight - (bucket.sizeBucket + 1) * bucketHeight;
    const QColor color = getColorForCount(bucket.allocationCount);
    painter.fillRect(x, y, 1, bucketHeight, color);
  }

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
  updateVisualization();
}
