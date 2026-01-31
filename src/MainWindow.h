#pragma once

#include <QMainWindow>
#include "WaterfallWidget.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
    
public:
    explicit MainWindow(QWidget* parent = nullptr);
    
private:
    void loadData();
    
    WaterfallWidget* waterfallWidget_;
};
