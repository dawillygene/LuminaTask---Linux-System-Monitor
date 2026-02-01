# LuminaTask - Linux System Monitor

A professional system monitor application built with C++ and Qt 6 for Linux process management.

## Features

### Core Features
- **Real-time Process Monitoring**: View all running processes with PID, name, and memory usage
- **Process Management**: Safely terminate processes with confirmation dialogs
- **Dual Termination Methods**:
  - **Graceful Termination** (SIGTERM): Allows processes to clean up before exiting
  - **Force Termination** (SIGKILL): Immediate termination for frozen processes
- **Professional UI**: Clean Qt-based interface with context menus and status updates
- **Auto-refresh**: Configurable automatic process list updates
- **Permission-aware**: Only allows killing processes owned by the current user (unless running as root)

### Advanced Features

#### 🧊 Process Suspension (Deep Use Management)
- **Suspend Process**: Freeze applications with SIGSTOP to stop CPU usage while preserving memory
- **Resume Process**: Restore suspended applications with SIGCONT
- **Visual Indicators**: ❄️ Suspended processes shown with blue coloring
- **Memory Preservation**: Apps stay in RAM but don't consume CPU cycles

#### 🔍 Memory Leak Detection
- **Real-time Monitoring**: Tracks memory growth over time for each process
- **Leak Alerts**: Automatic detection of processes growing >100MB per minute
- **Visual Warnings**: ⚠️ Memory leaking processes highlighted in orange
- **Alert Dialogs**: Pop-up warnings with detailed leak information

#### 🎮 Focus Mode (Game Mode)
- **Performance Optimization**: Automatically boosts foreground application priority
- **Background Suppression**: Lowers priority of system services and background tasks
- **Smart Detection**: Identifies active application based on CPU usage patterns
- **Visual Feedback**: Green button indicator when Focus Mode is active

#### 📊 Enhanced Process Information
- **Process Grouping**: Multiple instances of same app grouped by name with total memory
- **Priority Display**: Visual priority indicators (🔥 High, ⚖️ Normal, 🐌 Low)
- **State Tracking**: Shows process states (Running/Suspended) with color coding
- **CPU Monitoring**: Real-time CPU usage calculation from `/proc/[PID]/stat`

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

### Advanced Features

#### Process Suspension
- **Suspend Process**: Right-click → "Suspend Process" to freeze an application
- **Resume Process**: Right-click → "Resume Process" to restore a suspended application
- **Use Case**: Keep multiple browser tabs open but freeze unused ones to save CPU

#### Memory Leak Detection
- **Automatic Alerts**: Pop-up warnings when processes show abnormal memory growth
- **Visual Indicators**: Look for ⚠️ and orange coloring in the Priority column
- **Prevention**: Use Suspend or Kill options when leaks are detected

#### Focus Mode (Game Mode)
- **Enable**: Click the "Focus Mode" button (turns green when active)
- **Benefits**: Automatically optimizes system for your active application
- **How it works**: Boosts foreground app priority, lowers background task priorities

### Safety Features
- **Confirmation Dialogs**: All termination actions require user confirmation
- **Permission Checks**: Can only terminate processes owned by the current user
- **Root Warning**: Displays a warning when running with elevated privileges
- **Memory Leak Alerts**: Warns before processes consume excessive system resources

## Advanced Features Guide

### Process Suspension (Deep Use Management)
LuminaTask implements intelligent process suspension to prevent apps from consuming resources when not actively used.

**How it works:**
- **SIGSTOP**: Freezes the process completely (no CPU usage)
- **Memory Preservation**: Process stays in RAM but becomes inactive
- **SIGCONT**: Resumes the process exactly where it left off

**Use Cases:**
- Keep multiple browser tabs open but freeze unused ones
- Suspend background applications during gaming
- Prevent memory leaks from consuming system resources

**Visual Indicators:**
- ❄️ Suspended processes shown in blue
- ▶️ Running processes shown in green

### Memory Leak Detection
Automatic detection of processes with abnormal memory growth patterns.

**Detection Algorithm:**
- Monitors memory usage every 2 seconds
- Tracks 1-minute history per process
- Flags processes growing >100MB per minute
- Normalizes growth rate for accurate detection

**Alert System:**
- Pop-up warnings with process details
- Memory growth statistics
- Options to suspend or terminate problematic processes

### Focus Mode (Performance Optimization)
Game Mode-like functionality that optimizes system performance for active applications.

**Features:**
- **Priority Boost**: Active application gets high priority (-10 nice value)
- **Background Suppression**: System services and background tasks get low priority (+10)
- **Smart Detection**: Automatically identifies foreground application
- **Visual Feedback**: Green button when active

**Benefits:**
- Smoother gaming experience
- Better performance for active applications
- Reduced system lag during intensive tasks

### Process Grouping & Visualization
Enhanced process display with intelligent grouping and visual indicators.

**Tree Structure:**
- **Groups**: Applications with multiple instances (e.g., Chrome tabs)
- **Totals**: Combined memory and CPU usage for groups
- **Individuals**: Expandable child items for each process instance

**Visual Priority System:**
- 🔥 **High Priority**: Red (nice < -5)
- ⚖️ **Normal Priority**: Gray (-5 ≤ nice ≤ 5)
- 🐌 **Low Priority**: Gray (nice > 5)
- ⚠️ **Memory Leak**: Orange warning indicator

### Code Structure
```
LuminaTask/
├── CMakeLists.txt          # Build configuration
├── README.md              # This file
├── RECOMMENDED_FEATURES.md # Feature roadmap and proposals
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
- **NEW**: Process suspension/resume with SIGSTOP/SIGCONT
- **NEW**: Memory leak detection with historical tracking
- **NEW**: Focus mode with automatic priority optimization
- **NEW**: Real-time CPU usage calculation
- Provides real-time update signals

#### MainWindow
- Qt-based GUI implementation
- Tree view for hierarchical process display
- Context menu and toolbar management
- **NEW**: Focus mode toggle button with visual feedback
- **NEW**: Memory leak alert dialogs
- **NEW**: Priority and state column displays
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