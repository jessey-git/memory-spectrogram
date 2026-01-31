#include "CSVDataSource.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>

CSVDataSource::CSVDataSource(const QString& filePath)
    : filePath_(filePath)
{
}

std::vector<AllocationEvent> CSVDataSource::loadData()
{
    std::vector<AllocationEvent> events;
    
    QFile file(filePath_);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file:" << filePath_;
        return events;
    }
    
    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine();
        QStringList parts = line.split(',');
        
        if (parts.size() == 2) {
            bool ok1, ok2;
            double timeMs = parts[0].toDouble(&ok1);
            size_t size = parts[1].toULongLong(&ok2);
            
            if (ok1 && ok2) {
                events.push_back({timeMs, size});
            }
        }
    }
    
    file.close();
    return events;
}
