/* SPDX-FileCopyrightText: 2026 Jesse Yurkovich
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "ETWDataSource.h"

#include <QDebug>

#include <evntcons.h>
#include <tdh.h>

#pragma comment(lib, "tdh.lib")
#pragma comment(lib, "advapi32.lib")

QMutex ETWDataSource::dataMutex_;
AllocationEvents ETWDataSource::events_;
LARGE_INTEGER ETWDataSource::startTime_;
LARGE_INTEGER ETWDataSource::frequency_;
double ETWDataSource::firstTimestampMs_ = 0.0;
bool ETWDataSource::haveFirstTimestamp_ = false;
bool ETWDataSource::shouldStop_ = false;

static bool TryGetUInt64Property(PEVENT_RECORD pEvent,
                                 PTRACE_EVENT_INFO pInfo,
                                 const wchar_t *propertyName,
                                 ULONGLONG &outValue)
{
  for (DWORD i = 0; i < pInfo->TopLevelPropertyCount; i++) {
    const wchar_t *propName = (const wchar_t *)((PBYTE)pInfo +
                                                pInfo->EventPropertyInfoArray[i].NameOffset);
    if (wcscmp(propName, propertyName) != 0) {
      continue;
    }

    PROPERTY_DATA_DESCRIPTOR descriptor{};
    descriptor.PropertyName = ULONGLONG(propName);
    descriptor.ArrayIndex = ULONG_MAX;

    ULONG size = 0;
    ULONG status = TdhGetPropertySize(pEvent, 0, nullptr, 1, &descriptor, &size);
    if (status != ERROR_SUCCESS || size == 0 || size > 8) {
      return false;
    }

    BYTE buf[8];
    status = TdhGetProperty(pEvent, 0, nullptr, 1, &descriptor, size, buf);
    if (status != ERROR_SUCCESS) {
      return false;
    }

    if (size == sizeof(ULONG)) {
      outValue = *(ULONG *)buf;
      return true;
    }
    if (size == sizeof(ULONGLONG)) {
      outValue = *(ULONGLONG *)buf;
      return true;
    }
    if (size == sizeof(ULONG_PTR)) {
      outValue = *(ULONG_PTR *)buf;
      return true;
    }

    return false;
  }

  return false;
}

ETWDataSource::ETWDataSource(QObject *parent)
    : QObject(parent), sessionHandle_(0), consumerHandle_(0), running_(false)
{
  notifyTimer_ = new QTimer(this);
  connect(notifyTimer_, &QTimer::timeout, this, &ETWDataSource::newDataAvailable);

  QueryPerformanceFrequency(&frequency_);
}

ETWDataSource::~ETWDataSource()
{
  stop();
}

void WINAPI ETWDataSource::EventRecordCallback(PEVENT_RECORD pEvent)
{
  if (shouldStop_) {
    return;
  }

  constexpr UCHAR HEAP_OPCODE = 33;
  if (pEvent->EventHeader.EventDescriptor.Opcode != HEAP_OPCODE) {
    return;
  }

  DWORD bufferSize = 0;
  DWORD status = TdhGetEventInformation(pEvent, 0, nullptr, nullptr, &bufferSize);

  if (status == ERROR_INSUFFICIENT_BUFFER) {
    PTRACE_EVENT_INFO pInfo = (PTRACE_EVENT_INFO)malloc(bufferSize);
    if (pInfo == nullptr) {
      return;
    }

    status = TdhGetEventInformation(pEvent, 0, nullptr, pInfo, &bufferSize);
    if (status == ERROR_SUCCESS) {
      ULONGLONG allocSizeU64 = 0;
      if (TryGetUInt64Property(pEvent, pInfo, L"AllocSize", allocSizeU64)) {
        const double absoluteTimestampMs = pEvent->EventHeader.TimeStamp.QuadPart / 10000.0;

        QMutexLocker locker(&dataMutex_);

        if (!haveFirstTimestamp_) {
          QueryPerformanceCounter(&startTime_);
          firstTimestampMs_ = absoluteTimestampMs;
          haveFirstTimestamp_ = true;
        }

        const double timestampMs = (absoluteTimestampMs >= firstTimestampMs_) ?
                                       (absoluteTimestampMs - firstTimestampMs_) :
                                       0.0;

        events_.push_back(AllocationEvent{timestampMs, allocSizeU64});
      }
    }

    free(pInfo);
  }
}

DWORD WINAPI ETWDataSource::ProcessTraceThreadProc(LPVOID param)
{
  TRACEHANDLE handle = *(TRACEHANDLE *)param;
  ProcessTrace(&handle, 1, nullptr, nullptr);
  return 0;
}

bool ETWDataSource::start()
{
  if (running_) {
    return true;
  }

  wchar_t sessionName[] = L"MemoryWaterfallSession";
  ULONG bufferSize = sizeof(EVENT_TRACE_PROPERTIES) +
                     ULONG((wcslen(sessionName) + 1) * sizeof(wchar_t));
  PEVENT_TRACE_PROPERTIES pSessionProperties = (PEVENT_TRACE_PROPERTIES)malloc(bufferSize);

  if (pSessionProperties == nullptr) {
    emit errorOccurred("Failed to allocate session properties");
    return false;
  }

  ZeroMemory(pSessionProperties, bufferSize);
  pSessionProperties->Wnode.BufferSize = bufferSize;
  pSessionProperties->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
  pSessionProperties->Wnode.ClientContext = 1;
  pSessionProperties->Wnode.Guid = GUID_NULL;
  pSessionProperties->LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
  pSessionProperties->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);

  wcscpy_s((wchar_t *)((PBYTE)pSessionProperties + pSessionProperties->LoggerNameOffset),
           (bufferSize - pSessionProperties->LoggerNameOffset) / sizeof(wchar_t),
           sessionName);

  ControlTrace(0, sessionName, pSessionProperties, EVENT_TRACE_CONTROL_STOP);

  ULONG status = StartTrace(&sessionHandle_, sessionName, pSessionProperties);
  if (status != ERROR_SUCCESS) {
    free(pSessionProperties);
    emit errorOccurred(
        QString("Failed to start trace session. Error: %1. Requires Administrator privileges.")
            .arg(status));
    return false;
  }

  // Heap Provider GUID: {222962AB-6180-4B88-A825-346B75F2A24A}
  constexpr GUID HeapProviderGuid = {
      0x222962AB, 0x6180, 0x4B88, {0xA8, 0x25, 0x34, 0x6B, 0x75, 0xF2, 0xA2, 0x4A}};
  status = EnableTraceEx2(sessionHandle_,
                          &HeapProviderGuid,
                          EVENT_CONTROL_CODE_ENABLE_PROVIDER,
                          TRACE_LEVEL_INFORMATION,
                          0xFFFFFFFFFFFFFFFFULL,
                          0,
                          0,
                          nullptr);
  if (status != ERROR_SUCCESS) {
    ControlTrace(sessionHandle_, nullptr, pSessionProperties, EVENT_TRACE_CONTROL_STOP);
    free(pSessionProperties);
    emit errorOccurred(QString("Failed to enable heap provider. Error: %1").arg(status));
    return false;
  }

  EVENT_TRACE_LOGFILE traceLogfile;
  ZeroMemory(&traceLogfile, sizeof(EVENT_TRACE_LOGFILE));
  traceLogfile.LoggerName = sessionName;
  traceLogfile.ProcessTraceMode = PROCESS_TRACE_MODE_REAL_TIME | PROCESS_TRACE_MODE_EVENT_RECORD;
  traceLogfile.EventRecordCallback = EventRecordCallback;

  consumerHandle_ = OpenTrace(&traceLogfile);
  if (consumerHandle_ == INVALID_PROCESSTRACE_HANDLE) {
    ControlTrace(sessionHandle_, nullptr, pSessionProperties, EVENT_TRACE_CONTROL_STOP);
    free(pSessionProperties);
    emit errorOccurred(QString("Failed to open trace. Error: %1").arg(GetLastError()));
    return false;
  }

  free(pSessionProperties);

  shouldStop_ = false;
  haveFirstTimestamp_ = false;

  {
    QMutexLocker locker(&dataMutex_);
    events_.clear();
  }

  processThread_ = std::thread(ProcessTraceThreadProc, &consumerHandle_);

  running_ = true;
  notifyTimer_->start(50);

  qDebug() << "ETW tracing started successfully";
  return true;
}

void ETWDataSource::stop()
{
  if (!running_) {
    return;
  }

  notifyTimer_->stop();
  shouldStop_ = true;

  if (consumerHandle_) {
    CloseTrace(consumerHandle_);
    consumerHandle_ = 0;
  }

  if (processThread_.joinable()) {
    processThread_.join();
  }

  cleanupSession();
  running_ = false;

  qDebug() << "ETW tracing stopped";
}

void ETWDataSource::cleanupSession()
{
  if (sessionHandle_) {
    wchar_t sessionName[] = L"MemoryWaterfallSession";
    ULONG bufferSize = sizeof(EVENT_TRACE_PROPERTIES) +
                       ULONG((wcslen(sessionName) + 1) * sizeof(wchar_t));
    PEVENT_TRACE_PROPERTIES pSessionProperties = (PEVENT_TRACE_PROPERTIES)malloc(bufferSize);

    if (pSessionProperties) {
      ZeroMemory(pSessionProperties, bufferSize);
      pSessionProperties->Wnode.BufferSize = bufferSize;
      pSessionProperties->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);

      ControlTrace(sessionHandle_, nullptr, pSessionProperties, EVENT_TRACE_CONTROL_STOP);
      free(pSessionProperties);
    }

    sessionHandle_ = 0;
  }
}

AllocationEvents ETWDataSource::getRecentEvents(double maxAgeMs)
{
  QMutexLocker locker(&dataMutex_);

  if (events_.empty()) {
    return {};
  }

  const double latestTime = events_.back().timeMs;
  const double cutoffTime = latestTime - maxAgeMs;

  // Remove old events when at least half of the events are before the cutoff
  const int idealHalfSize = 512 * 1024;

  AllocationEvents recent;
  recent.reserve(idealHalfSize);

  bool cutoffIndexSet = false;
  size_t cutoffIndex = 0;
  for (size_t i = 0; i < events_.size(); ++i) {
    const AllocationEvent &event = events_[i];
    if (event.timeMs >= cutoffTime) {
      recent.push_back(event);
      if (recent.size() == 1 && i > idealHalfSize) {
        cutoffIndex = i - 1;
        cutoffIndexSet = true;
      }
    }
  }

  if (cutoffIndexSet) {
    events_.erase(events_.begin(), events_.begin() + cutoffIndex + 1);
  }

  return recent;
}

double ETWDataSource::getElapsedTimeMs() const
{
  QMutexLocker locker(&dataMutex_);

  if (!running_ || !haveFirstTimestamp_) {
    return 0.0;
  }

  LARGE_INTEGER currentTime;
  QueryPerformanceCounter(&currentTime);

  const double elapsedSeconds = double(currentTime.QuadPart - startTime_.QuadPart) /
                                frequency_.QuadPart;
  return elapsedSeconds * 1000.0;
}
