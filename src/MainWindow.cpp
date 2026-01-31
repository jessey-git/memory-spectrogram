#include "MainWindow.h"
#include "CSVDataSource.h"
#include "DataProcessor.h"
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QMessageBox>
#include <QScrollArea>
#include <cstdio>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("Memory Waterfall Viewer");
    
    QScrollArea* scrollArea = new QScrollArea(this);
    waterfallWidget_ = new WaterfallWidget();
    scrollArea->setWidget(waterfallWidget_);
    scrollArea->setWidgetResizable(false);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    
    setCentralWidget(scrollArea);
    
    QMenu* fileMenu = menuBar()->addMenu("&File");
    QAction* openAction = fileMenu->addAction("&Open CSV...");
    connect(openAction, &QAction::triggered, this, &MainWindow::loadData);
    
    QAction* exitAction = fileMenu->addAction("E&xit");
    connect(exitAction, &QAction::triggered, this, &QMainWindow::close);
    
    CSVDataSource dataSource("mem.csv");
    auto events = dataSource.loadData();
    
    if (!events.empty()) {
        DataProcessor processor(5.0);
        auto bucketData = processor.procesData(events);
        waterfallWidget_->setData(bucketData);
    }
}

void MainWindow::loadData()
{
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "Open CSV File",
        "",
        "CSV Files (*.csv);;All Files (*)"
    );
    
    if (fileName.isEmpty()) {
        return;
    }

    printf("Trying to load %s\n", qPrintable(fileName));    
    CSVDataSource dataSource(fileName);

    auto events = dataSource.loadData();
    
    if (events.empty()) {
        QMessageBox::warning(this, "Error", "Failed to load data from file");
        return;
    }
    
    DataProcessor processor(5.0);
    auto bucketData = processor.procesData(events);
    waterfallWidget_->setData(bucketData);
}
