/* SPDX-FileCopyrightText: 2026 Jesse Yurkovich
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <vector>

struct AllocationEvent {
  double timeMs;
  size_t size;
};

struct AllocationStats {
  size_t totalAllocations = 0;
  size_t totalSize = 0;
  size_t maxTimeBucketAllocationCount = 0;
  size_t maxSize = 0;
  double timeBucketMs = 0.0;
};

using AllocationEvents = std::vector<AllocationEvent>;
