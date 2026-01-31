/* SPDX-FileCopyrightText: 2026 Jesse Yurkovich
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "DataSource.h"

#include <QPixmap>
#include <QWidget>

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
  void setCurrentTime(double timeMs);
  QSize sizeHint() const override;

  void updateLiveData(AllocationEvents &&events);

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
  bool liveMode_;
  double currentTimeMs_;

  static constexpr double MAX_TIME_WINDOW_MS = 30000.0;

  std::vector<QColor> viridisColors_;
};
