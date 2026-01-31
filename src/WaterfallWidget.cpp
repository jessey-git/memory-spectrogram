#include "WaterfallWidget.h"
#include <QPainter>
#include <QScrollBar>
#include <QVBoxLayout>
#include <algorithm>

WaterfallWidget::WaterfallWidget(QWidget* parent)
  : QWidget(parent)
{
  viridisColors_.resize(101);
  for (int i = 0; i <= 100; ++i) {
    double t = i / 100.0;

    double r = 0.267004 + t * (0.329415 - 0.267004 + t * (0.993248 - 0.329415));
    double g = 0.004874 + t * (0.718387 - 0.004874 + t * (0.906157 - 0.718387));
    double b = 0.329415 + t * (0.525653 - 0.329415 + t * (0.143936 - 0.525653));

    if (t < 0.5) {
      r = 0.267004 + 2 * t * (0.127568 - 0.267004);
      g = 0.004874 + 2 * t * (0.566949 - 0.004874);
      b = 0.329415 + 2 * t * (0.550556 - 0.329415);
    }
    else {
      double t2 = 2 * (t - 0.5);
      r = 0.127568 + t2 * (0.993248 - 0.127568);
      g = 0.566949 + t2 * (0.906157 - 0.566949);
      b = 0.550556 + t2 * (0.143936 - 0.550556);
    }

    viridisColors_[i] = QColor(
      static_cast<int>(r * 255),
      static_cast<int>(g * 255),
      static_cast<int>(b * 255)
    );
  }
}

void WaterfallWidget::setData(const std::vector<BucketData>& data)
{
  data_ = data;
  qWarning() << "GOT DATA";
  updateMyPixmap();
  update();
}

QColor WaterfallWidget::getColorForCount(int count) const
{
  if (count < 0) count = 0;
  if (count > 100) count = 100;
  return viridisColors_[count];
}

void WaterfallWidget::updateMyPixmap()
{
  qWarning() << "RUNNING WaterfallWidget::updateMyPixmap()";
  if (data_.empty()) {
    return;
  }

  int maxTimeBucket = 0;
  int maxSizeBucket = 0;

  for (const auto& bucket : data_) {
    maxTimeBucket = std::max(maxTimeBucket, bucket.timeBucket);
    maxSizeBucket = std::max(maxSizeBucket, bucket.sizeBucket);
  }

  int numSizeBuckets = maxSizeBucket + 1;
  int numTimeBuckets = maxTimeBucket + 1;

  int pixmapWidth = numTimeBuckets * BUCKET_WIDTH;
  int pixmapHeight = numSizeBuckets * BUCKET_HEIGHT;

  pixmap_ = QPixmap(pixmapWidth, pixmapHeight);
  pixmap_.fill(Qt::black);

  QPainter painter(&pixmap_);

  for (const auto& bucket : data_) {
    int x = bucket.timeBucket * BUCKET_WIDTH;
    int y = (numSizeBuckets - 1 - bucket.sizeBucket) * BUCKET_HEIGHT;

    QColor color = getColorForCount(bucket.allocationCount);
    painter.fillRect(x, y, BUCKET_WIDTH, BUCKET_HEIGHT, color);
  }
}

void WaterfallWidget::paintEvent(QPaintEvent* event)
{
  QPainter painter(this);

  if (!pixmap_.isNull()) {
    int yOffset = (height() - pixmap_.height()) / 2;
    if (yOffset < 0) yOffset = 0;

    painter.drawPixmap(0, yOffset, pixmap_);
  }
  else {
    painter.fillRect(rect(), Qt::black);
    painter.setPen(Qt::white);
    painter.drawText(rect(), Qt::AlignCenter, "No data loaded");
  }
}

void WaterfallWidget::resizeEvent(QResizeEvent* event)
{
  QWidget::resizeEvent(event);
}
