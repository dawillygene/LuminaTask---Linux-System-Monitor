#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTreeView>
#include <QStandardItemModel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QMessageBox>
#include <QAction>
#include <QMenu>
#include <QContextMenuEvent>
#include <memory>

#include "processmanager.h"

/**
 * @brief MainWindow class provides the GUI for the LuminaTask system monitor
 *
 * This class implements a professional Qt-based interface for process management
 * with real-time updates, context menus, and safe process termination.
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;

private slots:
    void onProcessesUpdated_(const QVector<ProcessInfo>& processes);
    void onProcessTerminated_(int pid, bool success);
    void onRefreshButtonClicked_();
    void onKillProcessAction_();
    void onKillGracefullyAction_();
    void onSuspendProcessAction_();
    void onResumeProcessAction_();
    void onAutoRefreshToggled_(bool enabled);
    void onFocusModeToggled_(bool enabled);
    void onMemoryLeakDetected_(int pid, const QString& processName, double growthMB);

private:
    // UI setup methods
    void setupUI_();
    void setupTreeView_();
    void setupToolbar_();
    void setupStatusBar_();
    void setupContextMenu_();

    // Table management
    void updateProcessTree_(const QVector<ProcessInfo>& processes);
    void clearProcessTree_();
    [[nodiscard]] int getSelectedProcessPID_() const;

    // UI helper methods
    void showErrorMessage_(const QString& title, const QString& message);
    void showConfirmationDialog_(int pid, const QString& processName, TerminationMethod method);

    // Member variables
    std::unique_ptr<QVBoxLayout> m_mainLayout;
    std::unique_ptr<QHBoxLayout> m_toolbarLayout;

    // Core UI components
    std::unique_ptr<QTreeView> m_processTreeView;
    std::unique_ptr<QStandardItemModel> m_processModel;
    std::unique_ptr<QPushButton> m_refreshButton;
    std::unique_ptr<QPushButton> m_autoRefreshButton;
    std::unique_ptr<QPushButton> m_focusModeButton;
    std::unique_ptr<QLabel> m_statusLabel;
    std::unique_ptr<QLabel> m_processCountLabel;

    // Process manager
    std::unique_ptr<ProcessManager> m_processManager;

    // Context menu
    std::unique_ptr<QMenu> m_contextMenu;
    std::unique_ptr<QAction> m_killProcessAction;
    std::unique_ptr<QAction> m_killGracefullyAction;
    std::unique_ptr<QAction> m_suspendProcessAction;
    std::unique_ptr<QAction> m_resumeProcessAction;

    // Constants
    static constexpr int TREE_COLUMN_NAME = 0;
    static constexpr int TREE_COLUMN_STATE = 1;
    static constexpr int TREE_COLUMN_MEMORY = 2;
    static constexpr int TREE_COLUMN_CPU = 3;
    static constexpr int TREE_COLUMN_PRIORITY = 4;
    static constexpr int TREE_COLUMN_PID = 5;
    static constexpr int TREE_COLUMN_COUNT = 6;
};

#endif // MAINWINDOW_H