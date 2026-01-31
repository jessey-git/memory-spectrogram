/* SPDX-FileCopyrightText: 2026 Jesse Yurkovich
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "MainWindow.h"
#include "CSVDataSource.h"

#include <QAction>
#include <QFileDialog>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), etwDataSource_(nullptr), updateTimer_(nullptr), isLiveCapture_(false)
{
  setWindowTitle("Memory Waterfall Viewer");

  waterfallWidget_ = new WaterfallWidget(this);
  setCentralWidget(waterfallWidget_);

  QMenu *fileMenu = menuBar()->addMenu("&File");
  QAction *openAction = fileMenu->addAction("&Open CSV...");
  connect(openAction, &QAction::triggered, this, &MainWindow::loadData);

  fileMenu->addSeparator();

  QAction *exitAction = fileMenu->addAction("E&xit");
  connect(exitAction, &QAction::triggered, this, &QMainWindow::close);

  QMenu *captureMenu = menuBar()->addMenu("&Capture");
  QAction *startCaptureAction = captureMenu->addAction("&Start Live Capture");
  connect(startCaptureAction, &QAction::triggered, this, &MainWindow::startLiveCapture);

  QAction *stopCaptureAction = captureMenu->addAction("S&top Live Capture");
  connect(stopCaptureAction, &QAction::triggered, this, &MainWindow::stopLiveCapture);

  statusBar()->showMessage("Ready");

  etwDataSource_ = new ETWDataSource(this);
  connect(etwDataSource_, &ETWDataSource::errorOccurred, this, [this](const QString &error) {
    QMessageBox::critical(this, "ETW Error", error);
    statusBar()->showMessage("ETW capture failed");
  });

  updateTimer_ = new QTimer(this);
  connect(updateTimer_, &QTimer::timeout, this, &MainWindow::updateFromETW);

  startLiveCapture();
}

MainWindow::~MainWindow()
{
  if (etwDataSource_ && etwDataSource_->isRunning()) {
    etwDataSource_->stop();
  }
}

void MainWindow::loadData()
{
  QString fileName = QFileDialog::getOpenFileName(
      this, "Open CSV File", "", "CSV Files (*.csv);;All Files (*)");

  if (fileName.isEmpty()) {
    return;
  }

  if (isLiveCapture_) {
    stopLiveCapture();
  }

  CSVDataSource dataSource(fileName);
  auto events = dataSource.loadData();

  if (events.empty()) {
    QMessageBox::warning(this, "Error", "Failed to load data from file");
    return;
  }

  waterfallWidget_->setData(events);
  statusBar()->showMessage(QString("Loaded %1 events from CSV").arg(events.size()));
}

void MainWindow::startLiveCapture()
{
  if (isLiveCapture_) {
    return;
  }

  if (etwDataSource_->start()) {
    isLiveCapture_ = true;
    waterfallWidget_->setLiveMode(true);
    updateTimer_->start(50);
    statusBar()->showMessage("Live ETW capture active");
  }
  else {
    statusBar()->showMessage("Failed to start ETW capture");
  }
}

void MainWindow::stopLiveCapture()
{
  if (!isLiveCapture_) {
    return;
  }

  updateTimer_->stop();
  etwDataSource_->stop();
  isLiveCapture_ = false;
  waterfallWidget_->setLiveMode(false);
  statusBar()->showMessage("Live capture stopped");
}

void MainWindow::updateFromETW()
{
  if (!isLiveCapture_) {
    return;
  }

  const double currentTime = etwDataSource_->getElapsedTimeMs();
  AllocationEvents events = etwDataSource_->getRecentEvents(30000.0);

  waterfallWidget_->updateLiveData(std::move(events));
  waterfallWidget_->setCurrentTime(currentTime);
}
