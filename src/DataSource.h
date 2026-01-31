/* SPDX-FileCopyrightText: 2026 Jesse Yurkovich
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <vector>

struct AllocationEvent {
  double timeMs;
  size_t size;
};

using AllocationEvents = std::vector<AllocationEvent>;
