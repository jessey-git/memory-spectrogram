/* SPDX-FileCopyrightText: 2026 Jesse Yurkovich
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "ETWDataSource.h"
#include "WaterfallWidget.h"

#include <QMainWindow>
#include <QTimer>

class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow() final;

 private slots:
  void loadData();
  void startLiveCapture();
  void stopLiveCapture();
  void updateFromETW();

 private:
  WaterfallWidget *waterfallWidget_;
  ETWDataSource *etwDataSource_;
  QTimer *updateTimer_;
  bool isLiveCapture_;
};
