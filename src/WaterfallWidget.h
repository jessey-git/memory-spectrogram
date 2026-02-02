/* SPDX-FileCopyrightText: 2026 Jesse Yurkovich
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "DataSource.h"

#include <QPixmap>
#include <QWidget>

#include <vector>

struct BucketData {
  int timeBucket;
  int sizeBucket;
  int allocationCount;
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
  std::vector<BucketData> processDataForCurrentSize();

  static int getSizeBucketIndex(size_t size);

  AllocationEvents events_;
  QPixmap pixmap_;
  double currentTimeMs_ = 0.0;
  bool liveMode_ = false;
};
