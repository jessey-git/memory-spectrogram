/* SPDX-FileCopyrightText: 2026 Jesse Yurkovich
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "DataSource.h"

#include <QPixmap>
#include <QWidget>

#include <algorithm>
#include <vector>

struct AllocationData {
  void prepare(int numTimeBuckets, int numSizeBuckets)
  {
    numTimeBuckets_ = numTimeBuckets;
    numSizeBuckets_ = numSizeBuckets;
    rawCounts_.resize(size_t(numTimeBuckets) * numSizeBuckets, 0);
    std::fill(rawCounts_.begin(), rawCounts_.end(), 0);
  }

  void incrementCount(int timeBucket, int sizeBucket)
  {
    rawCounts_[size_t(timeBucket) * numSizeBuckets_ + sizeBucket]++;
  }

  template<typename Fn> void process(Fn &&fn) const
  {
    for (int t = 0; t < numTimeBuckets_; ++t) {
      for (int s = 0; s < numSizeBuckets_; ++s) {
        const int count = rawCounts_[size_t(t) * numSizeBuckets_ + s];
        if (count > 0) {
          fn(t, s, count);
        }
      }
    }
  }

  int numTimeBuckets_ = 0;
  int numSizeBuckets_ = 0;
  std::vector<int> rawCounts_;
};

class WaterfallWidget : public QWidget {
  Q_OBJECT

 public:
  explicit WaterfallWidget(QWidget *parent = nullptr);

  void setData(const AllocationEvents &events);
  void setLiveMode(bool enabled);
  QSize sizeHint() const override;

  template<typename Fn> void updateLiveData(double timeMs, Fn &&fn)
  {
    currentTimeMs_ = timeMs;
    if (!liveMode_) {
      return;
    }
    fn(events_);
    updateVisualization();
  }

 protected:
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

 private:
  void updateVisualization();
  QColor getColorForCount(int count) const;
  void processDataForCurrentSize();

  const int StatsHeight = 25;

  AllocationEvents events_;
  AllocationData data_;
  AllocationStats stats_;
  QPixmap pixmap_;
  double currentTimeMs_ = 0.0;
  bool liveMode_ = false;
};
