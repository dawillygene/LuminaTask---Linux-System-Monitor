#ifndef PROCESSMANAGER_H
#define PROCESSMANAGER_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QTimer>
#include <QMap>
#include <QPair>
#include <optional>
#include <memory>
#include <chrono>
#include <csignal>

// Forward declarations
class QStandardItemModel;

/**
 * @brief Enumeration for process termination methods
 */
enum class TerminationMethod {
    Graceful,  // SIGTERM - allows process to clean up
    Force      // SIGKILL - immediate termination
};

/**
 * @brief Enumeration for process suspension states
 */
enum class ProcessState {
    Running,   // Normal running state
    Suspended  // Suspended with SIGSTOP
};

/**
 * @brief Structure containing process information
 */
struct ProcessInfo {
    int pid;
    QString name;
    double memoryMB;
    double cpuPercent;
    ProcessState state;
    QVector<QPair<qint64, double>> memoryHistory;  // timestamp, memory pairs
    bool isMemoryLeech;
    int priority;

    ProcessInfo() : pid(0), memoryMB(0.0), cpuPercent(0.0), state(ProcessState::Running), 
                   isMemoryLeech(false), priority(0) {}
    ProcessInfo(int p, const QString& n, double mem, double cpu = 0.0, ProcessState s = ProcessState::Running)
        : pid(p), name(n), memoryMB(mem), cpuPercent(cpu), state(s), isMemoryLeech(false), priority(0) {}
};

/**
 * @brief ProcessManager class handles all process-related operations
 *
 * This class follows the Single Responsibility Principle by focusing solely
 * on process discovery, information retrieval, and process management.
 */
class ProcessManager : public QObject {
    Q_OBJECT

public:
    explicit ProcessManager(QObject* parent = nullptr);
    ~ProcessManager() override;

    // Process discovery and information
    [[nodiscard]] QVector<ProcessInfo> getAllProcesses();
    [[nodiscard]] std::optional<ProcessInfo> getProcessInfo(int processID);

    // Process management
    [[nodiscard]] bool terminateProcess(int processID, TerminationMethod method = TerminationMethod::Graceful);
    [[nodiscard]] bool suspendProcess(int processID);
    [[nodiscard]] bool resumeProcess(int processID);
    [[nodiscard]] bool setPriority(int processID, int priority);
    
    // Memory leak detection
    void updateMemoryHistory_(ProcessInfo& processInfo);
    [[nodiscard]] bool detectMemoryLeak_(const ProcessInfo& processInfo) const;
    
    // Focus mode (Game mode)
    void enableFocusMode(bool enabled);
    [[nodiscard]] bool isFocusModeEnabled() const { return m_focusModeEnabled; }
    void optimizeForFocusedApp_();

    // Real-time updates
    void startPeriodicRefresh(std::chrono::milliseconds interval = std::chrono::milliseconds{2000});
    void stopPeriodicRefresh();

signals:
    void processesUpdated(const QVector<ProcessInfo>& processes);
    void processTerminated(int pid, bool success);
    void memoryLeakDetected(int pid, const QString& processName, double growthMB);
    void focusModeChanged(bool enabled);

private slots:
    void refreshProcessList_();

private:
    // Helper methods
    [[nodiscard]] bool isValidProcessID_(int pid) const;
    [[nodiscard]] QString readProcessName_(int pid) const;
    [[nodiscard]] double readProcessMemory_(int pid) const;
    [[nodiscard]] double readProcessCpu_(int pid) const;
    [[nodiscard]] ProcessState readProcessState_(int pid) const;
    [[nodiscard]] int readProcessPriority_(int pid) const;
    [[nodiscard]] bool canKillProcess_(int pid) const;
    [[nodiscard]] int getFocusedWindowPID_() const;
    [[nodiscard]] bool isBackgroundProcess_(const ProcessInfo& processInfo) const;

    // Member variables
    std::unique_ptr<QTimer> m_refreshTimer;
    mutable QVector<ProcessInfo> m_cachedProcesses;
    bool m_focusModeEnabled;
    QMap<int, QVector<QPair<qint64, double>>> m_processMemoryHistory;

    // Constants
    static constexpr int MAX_PROCESS_COUNT = 10000;
    static constexpr int REFRESH_INTERVAL_MS = 2000;
    static constexpr double MEMORY_LEAK_THRESHOLD_MB = 100.0;
    static constexpr qint64 MEMORY_LEAK_TIME_WINDOW_MS = 60000;  // 1 minute
    static constexpr int HISTORY_MAX_ENTRIES = 30;  // Keep 1 minute of history at 2-second intervals
};

// Custom exception for process operations
class ProcessException : public std::runtime_error {
public:
    explicit ProcessException(const QString& message)
        : std::runtime_error(message.toStdString()) {}
};

#endif // PROCESSMANAGER_H