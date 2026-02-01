# LuminaTask - Linux System Monitor

A professional system monitor application built with C++ and Qt 6 for Linux process management.

## Features

- **Real-time Process Monitoring**: View all running processes with PID, name, and memory usage
- **Process Management**: Safely terminate processes with confirmation dialogs
- **Dual Termination Methods**:
  - **Graceful Termination** (SIGTERM): Allows processes to clean up before exiting
  - **Force Termination** (SIGKILL): Immediate termination for frozen processes
- **Professional UI**: Clean Qt-based interface with context menus and status updates
- **Auto-refresh**: Configurable automatic process list updates
- **Permission-aware**: Only allows killing processes owned by the current user (unless running as root)

## Requirements

### System Requirements
- Linux OS (Ubuntu, Fedora, Arch, etc.)
- Qt 6 development libraries
- CMake 3.16+
- C++17 compatible compiler (GCC 9+)

### Dependencies

#### Ubuntu/Debian/Pop!_OS:
```bash
sudo apt update
sudo apt install build-essential qt6-base-dev qt6-declarative-dev cmake ninja-build
```

#### Fedora:
```bash
sudo dnf install qt6-qtbase-devel qt6-qtdeclarative-devel cmake ninja-build gcc-c++
```

#### Arch Linux:
```bash
sudo pacman -S base-devel qt6-base qt6-declarative cmake ninja
```

## Building

1. **Clone or navigate to the project directory**:
   ```bash
   cd LuminaTask
   ```

2. **Create build directory**:
   ```bash
   mkdir build && cd build
   ```

3. **Configure with CMake**:
   ```bash
   cmake .. -G Ninja
   ```

4. **Build the application**:
   ```bash
   ninja
   ```

5. **Run the application**:
   ```bash
   ./LuminaTask
   ```

## Usage

### Basic Operation
- **View Processes**: The main table shows all running processes with their PID, name, memory usage, and CPU percentage
- **Refresh**: Click the "Refresh" button to manually update the process list
- **Auto Refresh**: Toggle "Auto Refresh" for automatic updates every 2 seconds

### Process Management
- **Right-click** on any process row to access the context menu
- **Kill Gracefully**: Sends SIGTERM signal, allowing the process to clean up
- **Kill Process**: Sends SIGKILL signal for immediate termination

### Safety Features
- **Confirmation Dialogs**: All termination actions require user confirmation
- **Permission Checks**: Can only terminate processes owned by the current user
- **Root Warning**: Displays a warning when running with elevated privileges

## Architecture

### Code Structure
```
LuminaTask/
├── CMakeLists.txt          # Build configuration
├── README.md              # This file
├── src/
│   ├── main.cpp           # Application entry point
│   ├── processmanager.cpp # Core process management logic
│   └── mainwindow.cpp     # Qt UI implementation
└── include/
    ├── processmanager.h   # Process manager interface
    └── mainwindow.h       # Main window interface
```

### Key Classes

#### ProcessManager
- Handles `/proc` filesystem interaction
- Manages process discovery and information retrieval
- Implements safe process termination
- Provides real-time update signals

#### MainWindow
- Qt-based GUI implementation
- Table view for process display
- Context menu and toolbar management
- User interaction handling

## Development

### Code Standards
- **C++17** features and modern idioms
- **RAII** for resource management
- **Smart pointers** instead of raw pointers
- **Exception safety** and error handling
- **Qt naming conventions** and best practices

### Building for Development
```bash
# Debug build
cmake .. -DCMAKE_BUILD_TYPE=Debug -G Ninja
ninja

# Release build with optimizations
cmake .. -DCMAKE_BUILD_TYPE=Release -G Ninja
ninja
```

## Troubleshooting

### Common Issues

1. **Qt6 not found**:
   - Ensure Qt6 development packages are installed
   - Check `qmake6 --version`

2. **Permission denied when killing processes**:
   - Can only kill processes owned by current user
   - Run as root for system process management (use with caution)

3. **No processes displayed**:
   - Check `/proc` filesystem permissions
   - Ensure application has read access to `/proc/[PID]/` directories

4. **Build failures**:
   - Update CMake to version 3.16+
   - Verify all dependencies are installed
   - Check compiler compatibility (GCC 9+ recommended)

### Debug Mode
Enable debug logging by setting the environment variable:
```bash
export QT_LOGGING_RULES="*.debug=true"
./LuminaTask
```

## Contributing

1. Fork the repository
2. Create a feature branch
3. Follow the established code standards
4. Add tests for new functionality
5. Submit a pull request

## License

This project is open source. See LICENSE file for details.

## Author

**ELIA WILLIAM MARIKI (dawillygene)**  
GitHub: https://github.com/dawillygene

## Acknowledgments

- Built with Qt 6 framework
- Uses Linux `/proc` filesystem for system information
- Inspired by htop and Activity Monitor