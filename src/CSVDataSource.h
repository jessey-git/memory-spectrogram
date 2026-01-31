#pragma once

#include "DataSource.h"
#include <QString>

class CSVDataSource : public DataSource {
public:
    explicit CSVDataSource(const QString& filePath);
    std::vector<AllocationEvent> loadData() override;
    
private:
    QString filePath_;
};
