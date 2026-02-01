#include "mainwindow.h"

#include <QApplication>
#include <QDesktopServices>
#include <QHeaderView>
#include <QKeyEvent>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QStatusBar>
#include <QMessageBox>
#include <QAction>
#include <QMenu>
#include <QContextMenuEvent>
#include <QModelIndex>
#include <QStandardItem>
#include <QBrush>
#include <QFont>
#include <QTimer>
#include <QDebug>
#include <QIcon>
#include <QMap>
#include <algorithm>

/**
 * @brief Constructor for MainWindow
 */
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_mainLayout(std::make_unique<QVBoxLayout>())
    , m_toolbarLayout(std::make_unique<QHBoxLayout>())
    , m_processTreeView(std::make_unique<QTreeView>(this))
    , m_processModel(std::make_unique<QStandardItemModel>(this))
    , m_refreshButton(std::make_unique<QPushButton>("Refresh", this))
    , m_autoRefreshButton(std::make_unique<QPushButton>("Auto Refresh", this))
    , m_focusModeButton(std::make_unique<QPushButton>("Focus Mode", this))
    , m_statusLabel(std::make_unique<QLabel>("Ready", this))
    , m_processCountLabel(std::make_unique<QLabel>("Processes: 0", this))
    , m_processManager(std::make_unique<ProcessManager>(this))
    , m_contextMenu(std::make_unique<QMenu>(this))
    , m_killProcessAction(std::make_unique<QAction>("Kill Process", this))
    , m_killGracefullyAction(std::make_unique<QAction>("Kill Gracefully", this))
    , m_suspendProcessAction(std::make_unique<QAction>("Suspend Process", this))
    , m_resumeProcessAction(std::make_unique<QAction>("Resume Process", this)) {

    // Set window properties
    setWindowTitle("LuminaTask - Linux System Monitor");
    setMinimumSize(800, 600);
    resize(1200, 800);

    // Setup UI components
    setupUI_();
    setupTreeView_();
    setupToolbar_();
    setupStatusBar_();
    setupContextMenu_();

    // Connect signals and slots
    connect(m_processManager.get(), &ProcessManager::processesUpdated,
            this, &MainWindow::onProcessesUpdated_);
    connect(m_processManager.get(), &ProcessManager::processTerminated,
            this, &MainWindow::onProcessTerminated_);
    connect(m_refreshButton.get(), &QPushButton::clicked,
            this, &MainWindow::onRefreshButtonClicked_);
    connect(m_autoRefreshButton.get(), &QPushButton::toggled,
            this, &MainWindow::onAutoRefreshToggled_);
    connect(m_focusModeButton.get(), &QPushButton::toggled,
            this, &MainWindow::onFocusModeToggled_);
    connect(m_killProcessAction.get(), &QAction::triggered,
            this, &MainWindow::onKillProcessAction_);
    connect(m_killGracefullyAction.get(), &QAction::triggered,
            this, &MainWindow::onKillGracefullyAction_);
    connect(m_suspendProcessAction.get(), &QAction::triggered,
            this, &MainWindow::onSuspendProcessAction_);
    connect(m_resumeProcessAction.get(), &QAction::triggered,
            this, &MainWindow::onResumeProcessAction_);
    connect(m_processManager.get(), &ProcessManager::memoryLeakDetected,
            this, &MainWindow::onMemoryLeakDetected_);

    // Initial process list load
    onRefreshButtonClicked_();
}

/**
 * @brief Destructor for MainWindow
 */
MainWindow::~MainWindow() = default;

/**
 * @brief Handle context menu events
 */
void MainWindow::contextMenuEvent(QContextMenuEvent* event) {
    const QModelIndex index = m_processTreeView->indexAt(event->pos());
    if (index.isValid()) {
        m_processTreeView->selectionModel()->setCurrentIndex(
            index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        m_contextMenu->exec(event->globalPos());
    }
}

/**
 * @brief Setup the main UI layout
 */
void MainWindow::setupUI_() {
    // Create central widget
    QWidget* centralWidget = new QWidget(this);
    centralWidget->setLayout(m_mainLayout.get());
    setCentralWidget(centralWidget);

    // Add toolbar layout to main layout
    m_mainLayout->addLayout(m_toolbarLayout.get());

    // Add tree view to main layout
    m_mainLayout->addWidget(m_processTreeView.get());
}

/**
 * @brief Setup the process tree view
 */
void MainWindow::setupTreeView_() {
    // Set tree model
    m_processModel->setHorizontalHeaderLabels(
        {"Process Name", "State", "Memory (MB)", "CPU %", "Priority", "PID", "Count"});
    m_processTreeView->setModel(m_processModel.get());

    // Configure tree appearance
    m_processTreeView->setAlternatingRowColors(true);
    m_processTreeView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_processTreeView->setSortingEnabled(true);
    m_processTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_processTreeView->setRootIsDecorated(true);
    m_processTreeView->setItemsExpandable(true);

    // Configure column properties
    QHeaderView* header = m_processTreeView->header();
    header->setStretchLastSection(true);
    header->setSectionResizeMode(TREE_COLUMN_NAME, QHeaderView::Stretch);
    header->setSectionResizeMode(TREE_COLUMN_STATE, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(TREE_COLUMN_MEMORY, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(TREE_COLUMN_CPU, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(TREE_COLUMN_PRIORITY, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(TREE_COLUMN_PID, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(TREE_COLUMN_COUNT, QHeaderView::ResizeToContents);

    // Connect context menu signal
    connect(m_processTreeView.get(), &QTreeView::customContextMenuRequested,
            [this](const QPoint& pos) {
                QContextMenuEvent event(QContextMenuEvent::Mouse, pos,
                                      m_processTreeView->mapToGlobal(pos));
                contextMenuEvent(&event);
            });
}

/**
 * @brief Setup the toolbar
 */
void MainWindow::setupToolbar_() {
    // Configure buttons
    m_refreshButton->setIcon(QIcon::fromTheme("view-refresh"));
    m_autoRefreshButton->setCheckable(true);
    m_autoRefreshButton->setChecked(true);
    m_focusModeButton->setCheckable(true);
    m_focusModeButton->setChecked(false);
    m_focusModeButton->setIcon(QIcon::fromTheme("applications-games"));
    m_focusModeButton->setToolTip("Enable Focus Mode (Game Mode) - Optimizes system for foreground app");

    // Add buttons to toolbar layout
    m_toolbarLayout->addWidget(m_refreshButton.get());
    m_toolbarLayout->addWidget(m_autoRefreshButton.get());
    m_toolbarLayout->addWidget(m_focusModeButton.get());
    m_toolbarLayout->addStretch();
    m_toolbarLayout->addWidget(m_processCountLabel.get());

    // Set button sizes
    const QSize buttonSize(100, 30);
    m_refreshButton->setFixedSize(buttonSize);
    m_autoRefreshButton->setFixedSize(buttonSize);
    m_focusModeButton->setFixedSize(buttonSize);
}

/**
 * @brief Setup the status bar
 */
void MainWindow::setupStatusBar_() {
    statusBar()->addWidget(m_statusLabel.get());
    statusBar()->addPermanentWidget(m_processCountLabel.get());
}

/**
 * @brief Setup the context menu
 */
void MainWindow::setupContextMenu_() {
    // Configure actions
    m_killProcessAction->setIcon(QIcon::fromTheme("process-stop"));
    m_killGracefullyAction->setIcon(QIcon::fromTheme("system-shutdown"));
    m_suspendProcessAction->setIcon(QIcon::fromTheme("media-playback-pause"));
    m_resumeProcessAction->setIcon(QIcon::fromTheme("media-playback-start"));

    // Add actions to context menu
    m_contextMenu->addAction(m_suspendProcessAction.get());
    m_contextMenu->addAction(m_resumeProcessAction.get());
    m_contextMenu->addSeparator();
    m_contextMenu->addAction(m_killGracefullyAction.get());
    m_contextMenu->addAction(m_killProcessAction.get());
}

/**
 * @brief Handle processes updated signal
 */
void MainWindow::onProcessesUpdated_(const QVector<ProcessInfo>& processes) {
    updateProcessTree_(processes);
    m_statusLabel->setText("Processes updated");
}

/**
 * @brief Handle process terminated signal
 */
void MainWindow::onProcessTerminated_(int pid, bool success) {
    if (success) {
        m_statusLabel->setText(QString("Process %1 terminated successfully").arg(pid));
        // Refresh the list to show updated state
        onRefreshButtonClicked_();
    } else {
        showErrorMessage_("Termination Failed",
                         QString("Failed to terminate process %1").arg(pid));
    }
}

/**
 * @brief Handle refresh button clicked
 */
void MainWindow::onRefreshButtonClicked_() {
    m_statusLabel->setText("Refreshing process list...");
    const QVector<ProcessInfo> processes = m_processManager->getAllProcesses();
    updateProcessTree_(processes);
    m_statusLabel->setText("Process list refreshed");
}

/**
 * @brief Handle auto refresh toggle
 */
void MainWindow::onAutoRefreshToggled_(bool enabled) {
    if (enabled) {
        m_processManager->startPeriodicRefresh();
        m_statusLabel->setText("Auto refresh enabled");
    } else {
        m_processManager->stopPeriodicRefresh();
        m_statusLabel->setText("Auto refresh disabled");
    }
}

/**
 * @brief Handle kill process action
 */
void MainWindow::onKillProcessAction_() {
    const int pid = getSelectedProcessPID_();
    if (pid == -1) return;

    // Get process name for confirmation dialog
    const auto processInfo = m_processManager->getProcessInfo(pid);
    if (!processInfo.has_value()) {
        showErrorMessage_("Error", "Cannot get process information");
        return;
    }

    showConfirmationDialog_(pid, processInfo->name, TerminationMethod::Force);
}

/**
 * @brief Handle kill gracefully action
 */
void MainWindow::onKillGracefullyAction_() {
    const int pid = getSelectedProcessPID_();
    if (pid == -1) return;

    // Get process name for confirmation dialog
    const auto processInfo = m_processManager->getProcessInfo(pid);
    if (!processInfo.has_value()) {
        showErrorMessage_("Error", "Cannot get process information");
        return;
    }

    showConfirmationDialog_(pid, processInfo->name, TerminationMethod::Graceful);
}

/**
 * @brief Handle suspend process action
 */
void MainWindow::onSuspendProcessAction_() {
    const int pid = getSelectedProcessPID_();
    if (pid == -1) return;

    // Get process name for confirmation dialog
    const auto processInfo = m_processManager->getProcessInfo(pid);
    if (!processInfo.has_value()) {
        showErrorMessage_("Error", "Cannot get process information");
        return;
    }

    const QString question = QString("Are you sure you want to suspend process %1 (%2)?")
                           .arg(pid).arg(processInfo->name);

    const QMessageBox::StandardButton reply = QMessageBox::question(
        this, "Confirm Process Suspension", question,
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        const bool success = m_processManager->suspendProcess(pid);
        if (success) {
            m_statusLabel->setText(QString("Process %1 suspended successfully").arg(pid));
            onRefreshButtonClicked_();  // Refresh to show updated state
        } else {
            showErrorMessage_("Suspension Failed",
                             QString("Failed to suspend process %1").arg(pid));
        }
    }
}

/**
 * @brief Handle resume process action
 */
void MainWindow::onResumeProcessAction_() {
    const int pid = getSelectedProcessPID_();
    if (pid == -1) return;

    const bool success = m_processManager->resumeProcess(pid);
    if (success) {
        m_statusLabel->setText(QString("Process %1 resumed successfully").arg(pid));
        onRefreshButtonClicked_();  // Refresh to show updated state
    } else {
        showErrorMessage_("Resume Failed",
                         QString("Failed to resume process %1").arg(pid));
    }
}

/**
 * @brief Handle focus mode toggle
 */
void MainWindow::onFocusModeToggled_(bool enabled) {
    m_processManager->enableFocusMode(enabled);
    if (enabled) {
        m_statusLabel->setText("Focus Mode enabled - System optimized for foreground app");
        m_focusModeButton->setText("Focus Mode ON");
        m_focusModeButton->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; }");
    } else {
        m_statusLabel->setText("Focus Mode disabled - Normal process priorities restored");
        m_focusModeButton->setText("Focus Mode");
        m_focusModeButton->setStyleSheet("");
    }
}

/**
 * @brief Handle memory leak detection alert
 */
void MainWindow::onMemoryLeakDetected_(int pid, const QString& processName, double growthMB) {
    const QString message = QString("⚠️ Memory Leak Detected!\n\n"
                                   "Process: %1 (PID: %2)\n"
                                   "Memory growth: +%.1f MB in the last minute\n\n"
                                   "This process may be consuming excessive memory.\n"
                                   "Consider terminating or suspending it to prevent system instability.")
                                   .arg(processName).arg(pid).arg(growthMB);

    const QMessageBox::StandardButton reply = QMessageBox::warning(
        this, "Memory Leak Alert", message,
        QMessageBox::Ignore | QMessageBox::Open, QMessageBox::Open);

    if (reply == QMessageBox::Open) {
        // Find and select the problematic process in the tree
        // This would require implementing a search function in the tree view
        m_statusLabel->setText(QString("Memory leak detected in %1 (PID: %2)")
                              .arg(processName).arg(pid));
    }
}

/**
 * @brief Update the process tree with new data
 */
void MainWindow::updateProcessTree_(const QVector<ProcessInfo>& processes) {
    // Clear existing data
    clearProcessTree_();

    // Group processes by name
    QMap<QString, QVector<ProcessInfo>> processGroups;
    for (const auto& process : processes) {
        processGroups[process.name].append(process);
    }

    // Sort groups by total memory usage (descending)
    QVector<QPair<QString, QVector<ProcessInfo>>> sortedGroups;
    for (auto it = processGroups.begin(); it != processGroups.end(); ++it) {
        sortedGroups.append(qMakePair(it.key(), it.value()));
    }
    std::sort(sortedGroups.begin(), sortedGroups.end(),
              [](const QPair<QString, QVector<ProcessInfo>>& a,
                 const QPair<QString, QVector<ProcessInfo>>& b) {
                  double totalMemoryA = 0.0;
                  for (const auto& proc : a.second) totalMemoryA += proc.memoryMB;
                  double totalMemoryB = 0.0;
                  for (const auto& proc : b.second) totalMemoryB += proc.memoryMB;
                  return totalMemoryA > totalMemoryB;
              });

    // Add grouped process data
    for (const auto& group : sortedGroups) {
        const QString& processName = group.first;
        const QVector<ProcessInfo>& groupProcesses = group.second;

        // Calculate group totals
        double totalMemory = 0.0;
        double avgCpu = 0.0;
        for (const auto& proc : groupProcesses) {
            totalMemory += proc.memoryMB;
            avgCpu += proc.cpuPercent;
        }
        avgCpu /= groupProcesses.size();

        // Create parent item (group)
        QList<QStandardItem*> groupRow;
        QStandardItem* nameItem = new QStandardItem(processName);
        nameItem->setData("group", Qt::UserRole);  // Mark as group item
        groupRow << nameItem;

        QStandardItem* stateItem = new QStandardItem("");  // Empty for groups
        groupRow << stateItem;

        QStandardItem* memoryItem = new QStandardItem(QString::number(totalMemory, 'f', 2));
        memoryItem->setData(totalMemory, Qt::UserRole);
        groupRow << memoryItem;

        QStandardItem* cpuItem = new QStandardItem(QString::number(avgCpu, 'f', 1));
        cpuItem->setData(avgCpu, Qt::UserRole);
        groupRow << cpuItem;

        QStandardItem* priorityItem = new QStandardItem("");  // Empty for groups
        groupRow << priorityItem;

        QStandardItem* pidItem = new QStandardItem("");  // Empty for groups
        groupRow << pidItem;

        QStandardItem* countItem = new QStandardItem(QString::number(groupProcesses.size()));
        countItem->setData(groupProcesses.size(), Qt::UserRole);
        groupRow << countItem;

        m_processModel->appendRow(groupRow);

        // Add child items (individual processes)
        for (const auto& process : groupProcesses) {
            QList<QStandardItem*> processRow;
            QStandardItem* childNameItem = new QStandardItem("  " + process.name);
            childNameItem->setData(process.pid, Qt::UserRole);  // Store PID for context menu
            processRow << childNameItem;

            // State column with visual indicator
            QStandardItem* childStateItem = new QStandardItem();
            if (process.state == ProcessState::Suspended) {
                childStateItem->setText("❄️ Suspended");
                childStateItem->setForeground(QBrush(QColor(100, 150, 200)));  // Light blue color
            } else {
                childStateItem->setText("▶️ Running");
                childStateItem->setForeground(QBrush(QColor(50, 150, 50)));   // Green color
            }
            processRow << childStateItem;

            QStandardItem* childMemoryItem = new QStandardItem(QString::number(process.memoryMB, 'f', 2));
            childMemoryItem->setData(process.memoryMB, Qt::UserRole);
            processRow << childMemoryItem;

            QStandardItem* childCpuItem = new QStandardItem(QString::number(process.cpuPercent, 'f', 1));
            childCpuItem->setData(process.cpuPercent, Qt::UserRole);
            processRow << childCpuItem;

            // Priority column with visual indicator
            QStandardItem* childPriorityItem = new QStandardItem();
            QString priorityText;
            if (process.priority < -5) {
                priorityText = QString("🔥 High (%1)").arg(process.priority);
                childPriorityItem->setForeground(QBrush(QColor(255, 100, 100)));  // Red for high priority
            } else if (process.priority > 5) {
                priorityText = QString("🐌 Low (%1)").arg(process.priority);
                childPriorityItem->setForeground(QBrush(QColor(150, 150, 150)));   // Gray for low priority
            } else {
                priorityText = QString("⚖️ Normal (%1)").arg(process.priority);
                childPriorityItem->setForeground(QBrush(QColor(100, 100, 100)));
            }
            
            // Add memory leak warning indicator
            if (process.isMemoryLeech) {
                priorityText = "⚠️ " + priorityText + " (LEAK!)";
                childPriorityItem->setForeground(QBrush(QColor(255, 165, 0)));  // Orange for memory leak
            }
            
            childPriorityItem->setText(priorityText);
            processRow << childPriorityItem;

            QStandardItem* childPidItem = new QStandardItem(QString::number(process.pid));
            childPidItem->setData(process.pid, Qt::UserRole);
            processRow << childPidItem;

            QStandardItem* childCountItem = new QStandardItem("");  // Empty for individual processes
            processRow << childCountItem;

            groupRow.first()->appendRow(processRow);
        }
    }

    // Update process count
    m_processCountLabel->setText(QString("Processes: %1").arg(processes.size()));

    // Expand all groups by default
    m_processTreeView->expandAll();
}

/**
 * @brief Clear the process tree
 */
void MainWindow::clearProcessTree_() {
    m_processModel->clear();
    m_processModel->setHorizontalHeaderLabels(
        {"Process Name", "State", "Memory (MB)", "CPU %", "Priority", "PID", "Count"});
}

/**
 * @brief Get the PID of the currently selected process
 * @return PID or -1 if no selection or group selected
 */
int MainWindow::getSelectedProcessPID_() const {
    const QModelIndexList selectedRows = m_processTreeView->selectionModel()->selectedRows();
    if (selectedRows.isEmpty()) {
        return -1;
    }

    const QModelIndex index = selectedRows.first();
    if (!index.isValid()) {
        return -1;
    }

    // Check if this is a group item (parent) or individual process item (child)
    if (index.parent().isValid()) {
        // This is a child item (individual process)
        return m_processModel->itemFromIndex(index.siblingAtColumn(TREE_COLUMN_PID))->data(Qt::UserRole).toInt();
    } else {
        // This is a group item, don't allow termination of groups
        return -1;
    }
}

/**
 * @brief Show an error message dialog
 */
void MainWindow::showErrorMessage_(const QString& title, const QString& message) {
    QMessageBox::critical(this, title, message);
}

/**
 * @brief Show confirmation dialog for process termination
 */
void MainWindow::showConfirmationDialog_(int pid, const QString& processName, TerminationMethod method) {
    const QString methodText = (method == TerminationMethod::Graceful) ? "gracefully" : "forcefully";
    const QString question = QString("Are you sure you want to terminate process %1 (%2) %3?")
                           .arg(pid).arg(processName).arg(methodText);

    const QMessageBox::StandardButton reply = QMessageBox::question(
        this, "Confirm Process Termination", question,
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        const bool success = m_processManager->terminateProcess(pid, method);
        if (!success) {
            showErrorMessage_("Termination Failed",
                             QString("Failed to terminate process %1").arg(pid));
        }
    }
}