/* SPDX-FileCopyrightText: 2026 Jesse Yurkovich
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "DataSource.h"

#include <QMutex>
#include <QObject>
#include <QTimer>

#include <windows.h>

#include <evntrace.h>

#include <thread>

class ETWDataSource : public QObject {
  Q_OBJECT

 public:
  explicit ETWDataSource(QObject *parent = nullptr);
  ~ETWDataSource() final;

  bool start();
  void stop();
  bool isRunning() const
  {
    return running_;
  }

  double getElapsedTimeMs() const;

  void getRecentEvents(double maxAgeMs, AllocationEvents &recent) const;

 signals:
  void errorOccurred(const QString &error);

 private:
  void cleanupSession();

  std::thread processThread_;
  TRACEHANDLE sessionHandle_ = 0;
  TRACEHANDLE consumerHandle_ = 0;
  bool running_ = false;

  static void WINAPI EventRecordCallback(PEVENT_RECORD pEvent);
  static DWORD WINAPI ProcessTraceThreadProc(LPVOID param);

  static QMutex dataMutex_;
  static AllocationEvents events_;
  static LARGE_INTEGER startTime_;
  static LARGE_INTEGER frequency_;
  static double firstTimestampMs_;
  static bool haveFirstTimestamp_;
  static bool shouldStop_;
};
