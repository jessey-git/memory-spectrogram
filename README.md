# Memory Waterfall Viewer

A Qt-based C++ application that displays a horizontal spectrogram (waterfall graph) visualization of memory allocation patterns over time.

## Features

- Visualizes memory allocation frequency vs. time as a waterfall graph
- Reads allocation data from a live ETW heap tracing session or from static CSV files
- Viridis color map for allocation count visualization

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
59.529600, 2048
60.921000, 128
61.345900, 300
```

## ETW Trace Session

The application under inspection must first be set to allow heap tracing to occur:
```powershell
wpr.exe -HeapTracingConfig application_name.exe enable
```

After enabling heap tracing, you can start the MemoryWaterfall viewer and then the application.
Note: The tracing session in the viewer must be active _before_ the application starts.

When finished with analysis, you can disable heapt tracing:
```powershell
wpr.exe -HeapTracingConfig application_name.exe disable
```

## Architecture

### Data Flow
1. **CSVDataSource** - Implementation for CSV file reading
2. **ETWDataSource** - Implementation for ETW session reading
3. **WaterfallWidget** - Qt widget that renders the visualization
4. **MainWindow** - Main application window with menu and scrolling

### Visualization Details
- **Horizontal axis**: Time (left = oldest, right = newest)
- **Vertical axis**: Memory size buckets (bottom = smallest, top = largest)
- **Color**: Allocation count using Viridis color map (purple = 0, yellow = 200+)

## License

This project is open source and available under the GPL License v2.
