#pragma once

#include <QWidget>
#include <QPixmap>
#include <QScrollBar>
#include "DataProcessor.h"

class WaterfallWidget : public QWidget {
  Q_OBJECT

public:
  explicit WaterfallWidget(QWidget* parent = nullptr);

  void setData(const std::vector<BucketData>& data);

protected:
  void paintEvent(QPaintEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;

private:
  void updateMyPixmap();
  QColor getColorForCount(int count) const;

  std::vector<BucketData> data_;
  QPixmap pixmap_;
  QScrollBar* horizontalScrollBar_;

  static constexpr int BUCKET_HEIGHT = 10;
  static constexpr int BUCKET_WIDTH = 2;

  std::vector<QColor> viridisColors_;
};
