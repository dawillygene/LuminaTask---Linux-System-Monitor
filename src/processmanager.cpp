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
#include <pwd.h>
#include <grp.h>
#include <cstring>
#include <cerrno>
#include <cstdlib>

/**
 * @brief Constructor for ProcessManager
 */
ProcessManager::ProcessManager(QObject* parent)
    : QObject(parent)
    , m_refreshTimer(std::make_unique<QTimer>(this)) {

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
QVector<ProcessInfo> ProcessManager::getAllProcesses() const {
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
std::optional<ProcessInfo> ProcessManager::getProcessInfo(int processID) const {
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

        return ProcessInfo(processID, processName, memoryMB, cpuPercent);
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