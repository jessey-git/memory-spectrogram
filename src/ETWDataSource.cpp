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

static ULONGLONG GetAllocSize(PEVENT_RECORD pEvent, PTRACE_EVENT_INFO pInfo)
{
  // Note: We are assuming that the payload is at index 1 to avoid the cost of
  // searching through the properties by name.
  const wchar_t *propName = (const wchar_t *)((PBYTE)pInfo +
                                              pInfo->EventPropertyInfoArray[1].NameOffset);
  PROPERTY_DATA_DESCRIPTOR descriptor{};
  descriptor.PropertyName = ULONGLONG(propName);
  descriptor.ArrayIndex = ULONG_MAX;

  // Note: We expect this payload to be 8 bytes. We are taking a shortcut here
  // to avoid as much overhead as possible.
  BYTE buf[8];
  ULONG status = TdhGetProperty(pEvent, 0, nullptr, 1, &descriptor, 8, buf);
  if (status != ERROR_SUCCESS) {
    // Return a fixed positive value that will at least show something in the
    // visualizer (hopefully triggering further investigation).
    return 1;
  }

  return *(ULONGLONG *)buf;
}

ETWDataSource::ETWDataSource(QObject *parent) : QObject(parent)
{
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

  // Note: Emperically the size of the structure is 352 bytes. We don't want to
  // pay the cost of allocation on every event, so we use a stack buffer first.
  BYTE pInfoBuffer[384];
  DWORD bufferSize = sizeof(pInfoBuffer);

  PTRACE_EVENT_INFO pInfo = (PTRACE_EVENT_INFO)pInfoBuffer;
  DWORD status = TdhGetEventInformation(pEvent, 0, nullptr, pInfo, &bufferSize);

  if (status == ERROR_INSUFFICIENT_BUFFER) {
    pInfo = (PTRACE_EVENT_INFO)malloc(bufferSize);
    if (pInfo == nullptr) {
      return;
    }
    status = TdhGetEventInformation(pEvent, 0, nullptr, pInfo, &bufferSize);
  }

  if (status == ERROR_SUCCESS) {
    const double absoluteTimestampMs = pEvent->EventHeader.TimeStamp.QuadPart / 10000.0;
    const ULONGLONG allocSizeU64 = GetAllocSize(pEvent, pInfo);

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

  if (pInfo != (PTRACE_EVENT_INFO)pInfoBuffer) {
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

  qDebug() << "ETW tracing started successfully";
  return true;
}

void ETWDataSource::stop()
{
  if (!running_) {
    return;
  }

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

void ETWDataSource::getRecentEvents(double maxAgeMs, AllocationEvents &recent) const
{
  QMutexLocker locker(&dataMutex_);

  recent.clear();

  if (events_.empty()) {
    return;
  }

  const double latestTime = events_.back().timeMs;
  const double cutoffTime = latestTime - maxAgeMs;

  // Trigger cleanup once there's a meaningful buildup of old events
  const int cutoffSize = 512 * 1024;

  bool cutoffIndexSet = false;
  size_t cutoffIndex = 0;
  for (size_t i = 0; i < events_.size(); ++i) {
    const AllocationEvent &event = events_[i];
    if (event.timeMs >= cutoffTime) {
      recent.push_back(event);
      if (recent.size() == 1 && i > cutoffSize) {
        cutoffIndex = i - 1;
        cutoffIndexSet = true;
      }
    }
  }

  if (cutoffIndexSet) {
    events_.erase(events_.begin(), events_.begin() + cutoffIndex + 1);
  }
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
