#ifndef PROCESSMANAGER_H
#define PROCESSMANAGER_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QTimer>
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
 * @brief Structure containing process information
 */
struct ProcessInfo {
    int pid;
    QString name;
    double memoryMB;
    double cpuPercent;

    ProcessInfo() : pid(0), memoryMB(0.0), cpuPercent(0.0) {}
    ProcessInfo(int p, const QString& n, double mem, double cpu = 0.0)
        : pid(p), name(n), memoryMB(mem), cpuPercent(cpu) {}
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
    [[nodiscard]] QVector<ProcessInfo> getAllProcesses() const;
    [[nodiscard]] std::optional<ProcessInfo> getProcessInfo(int processID) const;

    // Process management
    [[nodiscard]] bool terminateProcess(int processID, TerminationMethod method = TerminationMethod::Graceful);

    // Real-time updates
    void startPeriodicRefresh(std::chrono::milliseconds interval = std::chrono::milliseconds{2000});
    void stopPeriodicRefresh();

signals:
    void processesUpdated(const QVector<ProcessInfo>& processes);
    void processTerminated(int pid, bool success);

private slots:
    void refreshProcessList_();

private:
    // Helper methods
    [[nodiscard]] bool isValidProcessID_(int pid) const;
    [[nodiscard]] QString readProcessName_(int pid) const;
    [[nodiscard]] double readProcessMemory_(int pid) const;
    [[nodiscard]] double readProcessCpu_(int pid) const;
    [[nodiscard]] bool canKillProcess_(int pid) const;

    // Member variables
    std::unique_ptr<QTimer> m_refreshTimer;
    mutable QVector<ProcessInfo> m_cachedProcesses;

    // Constants
    static constexpr int MAX_PROCESS_COUNT = 10000;
    static constexpr int REFRESH_INTERVAL_MS = 2000;
};

// Custom exception for process operations
class ProcessException : public std::runtime_error {
public:
    explicit ProcessException(const QString& message)
        : std::runtime_error(message.toStdString()) {}
};

#endif // PROCESSMANAGER_H