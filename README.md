# Memory Waterfall Viewer

A Qt-based C++ application that displays a horizontal spectrogram (waterfall graph) visualization of memory allocation patterns over time.

## Features

- Visualizes memory allocation frequency vs. time as a waterfall graph
- Reads allocation data from CSV files
- Configurable time buckets (default: 5ms)
- 32 predefined memory size buckets from 8 bytes to >49KB
- Viridis color map for allocation count visualization (0-100)
- Horizontal scrolling support for large datasets
- Extensible data source architecture

## Requirements

- C++20 compatible compiler
- CMake 3.20 or later
- Qt 6 (Core, Widgets, Gui modules)
- vcpkg (for Qt package management on Windows)

## Building on Windows

### Setting up vcpkg

If you haven't already, install vcpkg and Qt6:

```powershell
# Clone vcpkg
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat

# Install Qt6
.\vcpkg install qt6-base:x64-windows
```

### Configure and Build

```powershell
# Configure CMake with vcpkg toolchain
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE="[path-to-vcpkg]/scripts/buildsystems/vcpkg.cmake"

# Build
cmake --build build --config Release
```

### Run

```powershell
.\build\Release\MemoryWaterfall.exe
```

## CSV Data Format

The application expects CSV files with two columns (no header):
- Column 1: Elapsed time in milliseconds (double)
- Column 2: Memory allocation size in bytes (integer)

Example:
```
59.529600,2048
60.921000,128
61.345900,2024
```

## Architecture

### Data Flow
1. **DataSource** - Abstract interface for loading allocation events
2. **CSVDataSource** - Concrete implementation for CSV file reading
3. **DataProcessor** - Processes raw events into time/size buckets
4. **WaterfallWidget** - Qt widget that renders the visualization
5. **MainWindow** - Main application window with menu and scrolling

### Memory Size Buckets
The application uses 32 predefined bucket sizes (in bytes):
8, 16, 32, 48, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 448, 512, 640, 768, 896, 1024, 1792, 2688, 4032, 5376, 8192, 16448, 24640, 32832, 41024, 49216, >49216

### Visualization Details
- **Horizontal axis**: Time (left = oldest, right = newest)
- **Vertical axis**: Memory size buckets (bottom = smallest, top = largest)
- **Color**: Allocation count using Viridis color map (purple = 0, yellow = 100)
- **Bucket height**: 10 pixels
- **Time bucket width**: 2 pixels

## Extending Data Sources

To add a new data source, inherit from `DataSource` and implement the `loadData()` method:

```cpp
class MyDataSource : public DataSource {
public:
    std::vector<AllocationEvent> loadData() override {
        // Your implementation here
    }
};
```

## License

This project is open source and available under the MIT License.
