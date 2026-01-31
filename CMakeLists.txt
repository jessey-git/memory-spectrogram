cmake_minimum_required(VERSION 3.20)
project(MemoryWaterfall VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui)

add_executable(MemoryWaterfall
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/WaterfallWidget.cpp
    src/WaterfallWidget.h
    src/DataSource.h
    src/CSVDataSource.cpp
    src/CSVDataSource.h
    src/DataProcessor.cpp
    src/DataProcessor.h
)

target_link_libraries(MemoryWaterfall 
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
)

if(WIN32)
    set_target_properties(MemoryWaterfall PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()
