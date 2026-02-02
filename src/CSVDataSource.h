/* SPDX-FileCopyrightText: 2026 Jesse Yurkovich
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "DataSource.h"

#include <QString>

class CSVDataSource {
 public:
  explicit CSVDataSource(const QString &filePath);
  AllocationEvents loadData() const;

 private:
  QString filePath_;
};
