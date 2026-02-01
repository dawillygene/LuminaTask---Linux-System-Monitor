#include "processmanager.h"

#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QThread>
#include <QMutex>
#include <QMutexLocker>
#include <algorithm>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <pwd.h>
#include <grp.h>
#include <cstring>
#include <cerrno>
#include <cstdlib>
#include <QDateTime>
#include <QMap>

/**
 * @brief Constructor for ProcessManager
 */
ProcessManager::ProcessManager(QObject* parent)
    : QObject(parent)
    , m_refreshTimer(std::make_unique<QTimer>(this))
    , m_focusModeEnabled(false) {

    // Connect timer signal
    connect(m_refreshTimer.get(), &QTimer::timeout,
            this, &ProcessManager::refreshProcessList_);

    // Reserve space for cached processes
    m_cachedProcesses.reserve(MAX_PROCESS_COUNT);
}

/**
 * @brief Destructor for ProcessManager
 */
ProcessManager::~ProcessManager() {
    stopPeriodicRefresh();
}

/**
 * @brief Get information for all running processes
 * @return QVector of ProcessInfo structures
 */
QVector<ProcessInfo> ProcessManager::getAllProcesses() {
    QVector<ProcessInfo> processes;
    processes.reserve(MAX_PROCESS_COUNT);

    // Open /proc directory
    DIR* procDir = opendir("/proc");
    if (!procDir) {
        qWarning() << "Failed to open /proc directory:" << strerror(errno);
        return processes;
    }

    // Use RAII for directory handle
    std::unique_ptr<DIR, decltype(&closedir)> procDirGuard(procDir, closedir);

    struct dirent* entry;
    while ((entry = readdir(procDir)) != nullptr) {
        // Check if entry is a numeric directory (PID)
        const char* name = entry->d_name;
        if (!isValidProcessID_(std::atoi(name))) {
            continue;
        }

        const int pid = std::atoi(name);

        // Get process information
        auto processInfo = getProcessInfo(pid);
        if (processInfo.has_value()) {
            processes.append(processInfo.value());
        }
    }

    return processes;
}

/**
 * @brief Get information for a specific process
 * @param processID The process ID to query
 * @return Optional ProcessInfo structure
 */
std::optional<ProcessInfo> ProcessManager::getProcessInfo(int processID) {
    if (!isValidProcessID_(processID)) {
        qWarning() << "Invalid PID requested:" << processID;
        return std::nullopt;
    }

    const QString procPath = QString("/proc/%1").arg(processID);
    if (!QDir(procPath).exists()) {
        qDebug() << "Process" << processID << "no longer exists";
        return std::nullopt;
    }

    try {
        const QString processName = readProcessName_(processID);
        const double memoryMB = readProcessMemory_(processID);
        const double cpuPercent = readProcessCpu_(processID);
        const ProcessState state = readProcessState_(processID);
        const int priority = readProcessPriority_(processID);

        ProcessInfo processInfo(processID, processName, memoryMB, cpuPercent, state);
        processInfo.priority = priority;
        
        // Update memory history and detect leaks
        updateMemoryHistory_(processInfo);
        processInfo.isMemoryLeech = detectMemoryLeak_(processInfo);
        
        if (processInfo.isMemoryLeech) {
            const double growthMB = processInfo.memoryHistory.size() >= 2 ? 
                processInfo.memoryMB - processInfo.memoryHistory.first().second : 0.0;
            emit memoryLeakDetected(processID, processName, growthMB);
        }

        return processInfo;
    } catch (const ProcessException& e) {
        qWarning() << "Error reading process" << processID << ":" << e.what();
        return std::nullopt;
    }
}

/**
 * @brief Terminate a process
 * @param processID The process ID to terminate
 * @param method The termination method (graceful or force)
 * @return true if successful, false otherwise
 */
bool ProcessManager::terminateProcess(int processID, TerminationMethod method) {
    if (!isValidProcessID_(processID)) {
        qWarning() << "Invalid PID for termination:" << processID;
        return false;
    }

    if (!canKillProcess_(processID)) {
        qWarning() << "Cannot kill process" << processID << "(permission denied or doesn't exist)";
        return false;
    }

    const int signal = (method == TerminationMethod::Graceful) ? SIGTERM : SIGKILL;
    const int result = kill(processID, signal);

    if (result == 0) {
        qInfo() << "Successfully sent signal" << signal << "to process" << processID;
        emit processTerminated(processID, true);
        return true;
    } else {
        qWarning() << "Failed to terminate process" << processID << ":" << strerror(errno);
        emit processTerminated(processID, false);
        return false;
    }
}

/**
 * @brief Read process state from /proc/[PID]/stat
 * @param pid Process ID
 * @return ProcessState (Running or Suspended)
 */
ProcessState ProcessManager::readProcessState_(int pid) const {
    const QString statPath = QString("/proc/%1/stat").arg(pid);
    QFile statFile(statPath);

    if (!statFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return ProcessState::Running;  // Default to running if can't read
    }

    QTextStream stream(&statFile);
    const QString line = stream.readLine();

    if (line.isEmpty()) {
        return ProcessState::Running;
    }

    // Parse the stat line - process state is field 3 (after pid and comm)
    // Format: pid (comm) state ...
    const QStringList parts = line.split(' ', Qt::SkipEmptyParts);

    if (parts.size() < 3) {
        return ProcessState::Running;
    }

    // State field: R=running, S=sleeping, D=disk sleep, T=stopped, Z=zombie, etc.
    const QString state = parts[2];
    
    if (state == "T" || state == "t") {
        return ProcessState::Suspended;  // Process is stopped (SIGSTOP)
    }

    return ProcessState::Running;
}

/**
 * @brief Set process priority using setpriority
 * @param processID The process ID
 * @param priority Nice value (-20 to 19, lower = higher priority)
 * @return true if successful, false otherwise
 */
bool ProcessManager::setPriority(int processID, int priority) {
    if (!isValidProcessID_(processID)) {
        qWarning() << "Invalid PID for priority change:" << processID;
        return false;
    }

    // Clamp priority to valid range
    priority = qBound(-20, priority, 19);

    const int result = setpriority(PRIO_PROCESS, processID, priority);
    
    if (result == 0) {
        qInfo() << "Successfully set priority" << priority << "for process" << processID;
        return true;
    } else {
        qWarning() << "Failed to set priority for process" << processID << ":" << strerror(errno);
        return false;
    }
}

/**
 * @brief Update memory history for a process
 * @param processInfo Process information to update
 */
void ProcessManager::updateMemoryHistory_(ProcessInfo& processInfo) {
    const qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    
    // Get existing history for this process
    auto& history = m_processMemoryHistory[processInfo.pid];
    
    // Add current memory usage to history
    history.append(qMakePair(currentTime, processInfo.memoryMB));
    
    // Remove old entries (older than 1 minute)
    while (!history.isEmpty() && 
           (currentTime - history.first().first) > MEMORY_LEAK_TIME_WINDOW_MS) {
        history.removeFirst();
    }
    
    // Limit history size
    while (history.size() > HISTORY_MAX_ENTRIES) {
        history.removeFirst();
    }
    
    // Copy history to process info
    processInfo.memoryHistory = history;
}

/**
 * @brief Detect if a process is leaking memory
 * @param processInfo Process information to check
 * @return true if memory leak detected, false otherwise
 */
bool ProcessManager::detectMemoryLeak_(const ProcessInfo& processInfo) const {
    if (processInfo.memoryHistory.size() < 2) {
        return false;  // Need at least 2 data points
    }
    
    const auto& history = processInfo.memoryHistory;
    const qint64 currentTime = history.last().first;
    const double currentMemory = history.last().second;
    
    // Find memory usage from 1 minute ago (or closest available)
    double oldMemory = currentMemory;
    qint64 oldTime = currentTime;
    
    for (const auto& entry : history) {
        if ((currentTime - entry.first) >= MEMORY_LEAK_TIME_WINDOW_MS * 0.8) {  // 80% of window
            oldMemory = entry.second;
            oldTime = entry.first;
            break;
        }
    }
    
    // Calculate memory growth
    const double memoryGrowthMB = currentMemory - oldMemory;
    const qint64 timeSpanMS = currentTime - oldTime;
    
    // Check if growth exceeds threshold
    if (timeSpanMS > 0 && memoryGrowthMB > MEMORY_LEAK_THRESHOLD_MB) {
        // Normalize to 1-minute window
        const double normalizedGrowth = (memoryGrowthMB * MEMORY_LEAK_TIME_WINDOW_MS) / timeSpanMS;
        return normalizedGrowth > MEMORY_LEAK_THRESHOLD_MB;
    }
    
    return false;
}

/**
 * @brief Enable or disable focus mode
 * @param enabled True to enable focus mode, false to disable
 */
void ProcessManager::enableFocusMode(bool enabled) {
    if (m_focusModeEnabled == enabled) {
        return;  // No change
    }
    
    m_focusModeEnabled = enabled;
    
    if (enabled) {
        qInfo() << "Focus mode enabled";
        optimizeForFocusedApp_();
    } else {
        qInfo() << "Focus mode disabled";
        // Reset all process priorities to normal
        for (const auto& process : m_cachedProcesses) {
            (void)setPriority(process.pid, 0);  // Normal priority (ignore result)
        }
    }
    
    emit focusModeChanged(enabled);
}

/**
 * @brief Optimize system for focused application
 */
void ProcessManager::optimizeForFocusedApp_() {
    if (!m_focusModeEnabled) {
        return;
    }
    
    const int focusedPID = getFocusedWindowPID_();
    
    for (const auto& process : m_cachedProcesses) {
        if (process.pid == focusedPID) {
            // Boost focused app priority
            (void)setPriority(process.pid, -10);  // High priority (ignore result)
        } else if (isBackgroundProcess_(process)) {
            // Lower priority for background tasks
            (void)setPriority(process.pid, 10);   // Low priority (ignore result)
        }
    }
}

/**
 * @brief Read process priority from /proc/[PID]/stat
 * @param pid Process ID
 * @return Process nice value
 */
int ProcessManager::readProcessPriority_(int pid) const {
    const QString statPath = QString("/proc/%1/stat").arg(pid);
    QFile statFile(statPath);

    if (!statFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return 0;  // Default priority
    }

    QTextStream stream(&statFile);
    const QString line = stream.readLine();

    if (line.isEmpty()) {
        return 0;
    }

    // Parse the stat line - nice value is field 19
    const QStringList parts = line.split(' ', Qt::SkipEmptyParts);

    if (parts.size() < 19) {
        return 0;
    }

    bool ok;
    const int nice = parts[18].toInt(&ok);
    return ok ? nice : 0;
}

/**
 * @brief Get PID of currently focused window (simplified implementation)
 * @return PID of focused process, or 0 if unable to determine
 */
int ProcessManager::getFocusedWindowPID_() const {
    // Simplified implementation - in reality, this would use X11/Wayland APIs
    // For now, we'll use a heuristic: the process with highest CPU that's not a background task
    
    int focusedPID = 0;
    double highestCPU = 0.0;
    
    for (const auto& process : m_cachedProcesses) {
        if (!isBackgroundProcess_(process) && process.cpuPercent > highestCPU) {
            highestCPU = process.cpuPercent;
            focusedPID = process.pid;
        }
    }
    
    return focusedPID;
}

/**
 * @brief Check if a process is a background task
 * @param processInfo Process to check
 * @return true if it's a background process
 */
bool ProcessManager::isBackgroundProcess_(const ProcessInfo& processInfo) const {
    // Common background processes/services
    const QStringList backgroundProcesses = {
        "systemd", "kthreadd", "ksoftirqd", "rcu_", "watchdog",
        "systemd-", "dbus", "NetworkManager", "pulseaudio",
        "tracker", "baloo", "updatedb", "indexer", "backup",
        "cron", "anacron", "rsyslog", "accounts-daemon"
    };
    
    for (const QString& bgProcess : backgroundProcesses) {
        if (processInfo.name.contains(bgProcess, Qt::CaseInsensitive)) {
            return true;
        }
    }
    
    // Low CPU usage processes are likely background
    return processInfo.cpuPercent < 1.0 && processInfo.memoryMB > 50.0;
}

/**
 * @brief Suspend a process using SIGSTOP
 * @param processID The process ID to suspend
 * @return true if successful, false otherwise
 */
bool ProcessManager::suspendProcess(int processID) {
    if (!isValidProcessID_(processID)) {
        qWarning() << "Invalid PID for suspension:" << processID;
        return false;
    }

    if (!canKillProcess_(processID)) {
        qWarning() << "Cannot suspend process" << processID << "(permission denied or doesn't exist)";
        return false;
    }

    const int result = kill(processID, SIGSTOP);

    if (result == 0) {
        qInfo() << "Successfully suspended process" << processID;
        return true;
    } else {
        qWarning() << "Failed to suspend process" << processID << ":" << strerror(errno);
        return false;
    }
}

/**
 * @brief Resume a suspended process using SIGCONT
 * @param processID The process ID to resume
 * @return true if successful, false otherwise
 */
bool ProcessManager::resumeProcess(int processID) {
    if (!isValidProcessID_(processID)) {
        qWarning() << "Invalid PID for resumption:" << processID;
        return false;
    }

    if (!canKillProcess_(processID)) {
        qWarning() << "Cannot resume process" << processID << "(permission denied or doesn't exist)";
        return false;
    }

    const int result = kill(processID, SIGCONT);

    if (result == 0) {
        qInfo() << "Successfully resumed process" << processID;
        return true;
    } else {
        qWarning() << "Failed to resume process" << processID << ":" << strerror(errno);
        return false;
    }
}

/**
 * @brief Start periodic process list refresh
 * @param interval Refresh interval in milliseconds
 */
void ProcessManager::startPeriodicRefresh(std::chrono::milliseconds interval) {
    m_refreshTimer->start(interval.count());
}

/**
 * @brief Stop periodic process list refresh
 */
void ProcessManager::stopPeriodicRefresh() {
    if (m_refreshTimer->isActive()) {
        m_refreshTimer->stop();
    }
}

/**
 * @brief Slot called when refresh timer times out
 */
void ProcessManager::refreshProcessList_() {
    const QVector<ProcessInfo> processes = getAllProcesses();
    
    // Update cached processes for focus mode
    m_cachedProcesses = processes;
    
    // Optimize for focused app if focus mode is enabled
    if (m_focusModeEnabled) {
        optimizeForFocusedApp_();
    }
    
    emit processesUpdated(processes);
}

/**
 * @brief Validate if a process ID is valid
 * @param pid Process ID to validate
 * @return true if valid, false otherwise
 */
bool ProcessManager::isValidProcessID_(int pid) const {
    return pid > 0 && pid < 65536;
}

/**
 * @brief Read process name from /proc/[PID]/comm
 * @param pid Process ID
 * @return Process name as QString
 */
QString ProcessManager::readProcessName_(int pid) const {
    const QString commPath = QString("/proc/%1/comm").arg(pid);
    QFile commFile(commPath);

    if (!commFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        throw ProcessException(QString("Cannot open %1").arg(commPath));
    }

    QTextStream stream(&commFile);
    QString processName = stream.readLine().trimmed();

    if (processName.isEmpty()) {
        throw ProcessException("Empty process name");
    }

    return processName;
}

/**
 * @brief Read process memory usage from /proc/[PID]/status
 * @param pid Process ID
 * @return Memory usage in MB
 */
double ProcessManager::readProcessMemory_(int pid) const {
    const QString statusPath = QString("/proc/%1/status").arg(pid);
    QFile statusFile(statusPath);

    if (!statusFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        throw ProcessException(QString("Cannot open %1").arg(statusPath));
    }

    QTextStream stream(&statusFile);
    QString line;

    while (stream.readLineInto(&line)) {
        if (line.startsWith("VmRSS:")) {
            // Extract memory value (format: "VmRSS:    1234 kB")
            const QStringList parts = line.split(' ', Qt::SkipEmptyParts);
            if (parts.size() >= 3) {
                bool ok;
                const long kbValue = parts[1].toLong(&ok);
                if (ok) {
                    return kbValue / 1024.0;  // Convert KB to MB
                }
            }
            break;
        }
    }

    return 0.0;  // No memory information available
}

/**
 * @brief Read process CPU usage from /proc/[PID]/stat
 * @param pid Process ID
 * @return CPU usage percentage (0.0-100.0)
 */
double ProcessManager::readProcessCpu_(int pid) const {
    const QString statPath = QString("/proc/%1/stat").arg(pid);
    QFile statFile(statPath);

    if (!statFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return 0.0;
    }

    QTextStream stream(&statFile);
    const QString line = stream.readLine();

    if (line.isEmpty()) {
        return 0.0;
    }

    // Parse the stat line - CPU times are fields 14 (utime) and 15 (stime)
    // Format: pid (comm) state ppid ... utime stime ...
    const QStringList parts = line.split(' ', Qt::SkipEmptyParts);

    if (parts.size() < 16) {
        return 0.0;
    }

    bool ok1, ok2;
    const unsigned long utime = parts[13].toULong(&ok1);  // User time
    const unsigned long stime = parts[14].toULong(&ok2);  // System time

    if (!ok1 || !ok2) {
        return 0.0;
    }

    // Calculate total CPU time in clock ticks
    const unsigned long totalTime = utime + stime;

    // Get system uptime for percentage calculation
    QFile uptimeFile("/proc/uptime");
    if (!uptimeFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return 0.0;
    }

    QTextStream uptimeStream(&uptimeFile);
    const QString uptimeLine = uptimeStream.readLine();
    const QStringList uptimeParts = uptimeLine.split(' ', Qt::SkipEmptyParts);

    if (uptimeParts.isEmpty()) {
        return 0.0;
    }

    bool uptimeOk;
    const double uptime = uptimeParts[0].toDouble(&uptimeOk);

    if (!uptimeOk || uptime <= 0) {
        return 0.0;
    }

    // Get number of clock ticks per second
    const long ticksPerSecond = sysconf(_SC_CLK_TCK);
    if (ticksPerSecond <= 0) {
        return 0.0;
    }

    // Calculate CPU usage percentage
    // CPU% = (total_time / ticks_per_second) / uptime * 100
    const double cpuSeconds = static_cast<double>(totalTime) / ticksPerSecond;
    const double cpuPercent = (cpuSeconds / uptime) * 100.0;

    // Clamp to reasonable range (0-100%)
    return qBound(0.0, cpuPercent, 100.0);
}

/**
 * @brief Check if current user can kill the specified process
 * @param pid Process ID
 * @return true if can kill, false otherwise
 */
bool ProcessManager::canKillProcess_(int pid) const {
    if (geteuid() == 0) {
        // Root can kill any process
        return true;
    }

    // Check if process belongs to current user
    const QString statusPath = QString("/proc/%1/status").arg(pid);
    QFile statusFile(statusPath);

    if (!statusFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream stream(&statusFile);
    QString line;

    while (stream.readLineInto(&line)) {
        if (line.startsWith("Uid:")) {
            const QStringList parts = line.split('\t', Qt::SkipEmptyParts);
            if (parts.size() >= 2) {
                bool ok;
                const uid_t processUid = parts[1].toUInt(&ok);
                if (ok && processUid == geteuid()) {
                    return true;
                }
            }
            break;
        }
    }

    return false;
}