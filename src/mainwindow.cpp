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
#include <algorithm>

/**
 * @brief Constructor for MainWindow
 */
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_mainLayout(std::make_unique<QVBoxLayout>())
    , m_toolbarLayout(std::make_unique<QHBoxLayout>())
    , m_processTableView(std::make_unique<QTableView>(this))
    , m_processModel(std::make_unique<QStandardItemModel>(this))
    , m_refreshButton(std::make_unique<QPushButton>("Refresh", this))
    , m_autoRefreshButton(std::make_unique<QPushButton>("Auto Refresh", this))
    , m_statusLabel(std::make_unique<QLabel>("Ready", this))
    , m_processCountLabel(std::make_unique<QLabel>("Processes: 0", this))
    , m_processManager(std::make_unique<ProcessManager>(this))
    , m_contextMenu(std::make_unique<QMenu>(this))
    , m_killProcessAction(std::make_unique<QAction>("Kill Process", this))
    , m_killGracefullyAction(std::make_unique<QAction>("Kill Gracefully", this)) {

    // Set window properties
    setWindowTitle("LuminaTask - Linux System Monitor");
    setMinimumSize(800, 600);
    resize(1200, 800);

    // Setup UI components
    setupUI_();
    setupTableView_();
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
    connect(m_killProcessAction.get(), &QAction::triggered,
            this, &MainWindow::onKillProcessAction_);
    connect(m_killGracefullyAction.get(), &QAction::triggered,
            this, &MainWindow::onKillGracefullyAction_);

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
    const QModelIndex index = m_processTableView->indexAt(event->pos());
    if (index.isValid()) {
        m_processTableView->selectionModel()->setCurrentIndex(
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

    // Add table view to main layout
    m_mainLayout->addWidget(m_processTableView.get());
}

/**
 * @brief Setup the process table view
 */
void MainWindow::setupTableView_() {
    // Set table model
    m_processModel->setHorizontalHeaderLabels(
        {"PID", "Process Name", "Memory (MB)", "CPU %", "Actions"});
    m_processTableView->setModel(m_processModel.get());

    // Configure table appearance
    m_processTableView->setAlternatingRowColors(true);
    m_processTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_processTableView->setSortingEnabled(true);
    m_processTableView->setContextMenuPolicy(Qt::CustomContextMenu);

    // Configure column properties
    QHeaderView* header = m_processTableView->horizontalHeader();
    header->setStretchLastSection(true);
    header->setSectionResizeMode(TABLE_COLUMN_PID, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(TABLE_COLUMN_NAME, QHeaderView::Stretch);
    header->setSectionResizeMode(TABLE_COLUMN_MEMORY, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(TABLE_COLUMN_CPU, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(TABLE_COLUMN_ACTIONS, QHeaderView::ResizeToContents);

    // Connect context menu signal
    connect(m_processTableView.get(), &QTableView::customContextMenuRequested,
            [this](const QPoint& pos) {
                QContextMenuEvent event(QContextMenuEvent::Mouse, pos,
                                      m_processTableView->mapToGlobal(pos));
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

    // Add buttons to toolbar layout
    m_toolbarLayout->addWidget(m_refreshButton.get());
    m_toolbarLayout->addWidget(m_autoRefreshButton.get());
    m_toolbarLayout->addStretch();
    m_toolbarLayout->addWidget(m_processCountLabel.get());

    // Set button sizes
    const QSize buttonSize(100, 30);
    m_refreshButton->setFixedSize(buttonSize);
    m_autoRefreshButton->setFixedSize(buttonSize);
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

    // Add actions to context menu
    m_contextMenu->addAction(m_killGracefullyAction.get());
    m_contextMenu->addAction(m_killProcessAction.get());
}

/**
 * @brief Handle processes updated signal
 */
void MainWindow::onProcessesUpdated_(const QVector<ProcessInfo>& processes) {
    updateProcessTable_(processes);
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
    updateProcessTable_(processes);
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
 * @brief Update the process table with new data
 */
void MainWindow::updateProcessTable_(const QVector<ProcessInfo>& processes) {
    // Clear existing data
    clearProcessTable_();

    // Reserve space for efficiency
    m_processModel->setRowCount(processes.size());

    // Add process data
    for (int row = 0; row < processes.size(); ++row) {
        const ProcessInfo& process = processes[row];

        // PID
        QStandardItem* pidItem = new QStandardItem(QString::number(process.pid));
        pidItem->setData(process.pid, Qt::UserRole);  // Store PID for context menu
        m_processModel->setItem(row, TABLE_COLUMN_PID, pidItem);

        // Process Name
        QStandardItem* nameItem = new QStandardItem(process.name);
        m_processModel->setItem(row, TABLE_COLUMN_NAME, nameItem);

        // Memory
        QStandardItem* memoryItem = new QStandardItem(QString::number(process.memoryMB, 'f', 2));
        memoryItem->setData(process.memoryMB, Qt::UserRole);
        m_processModel->setItem(row, TABLE_COLUMN_MEMORY, memoryItem);

        // CPU (placeholder)
        QStandardItem* cpuItem = new QStandardItem(QString::number(process.cpuPercent, 'f', 1));
        cpuItem->setData(process.cpuPercent, Qt::UserRole);
        m_processModel->setItem(row, TABLE_COLUMN_CPU, cpuItem);

        // Actions (placeholder for future buttons)
        QStandardItem* actionsItem = new QStandardItem("Right-click for options");
        actionsItem->setEditable(false);
        m_processModel->setItem(row, TABLE_COLUMN_ACTIONS, actionsItem);
    }

    // Update process count
    m_processCountLabel->setText(QString("Processes: %1").arg(processes.size()));

    // Sort by PID by default
    m_processTableView->sortByColumn(TABLE_COLUMN_PID, Qt::AscendingOrder);
}

/**
 * @brief Clear the process table
 */
void MainWindow::clearProcessTable_() {
    m_processModel->clear();
    m_processModel->setHorizontalHeaderLabels(
        {"PID", "Process Name", "Memory (MB)", "CPU %", "Actions"});
}

/**
 * @brief Get the PID of the currently selected process
 * @return PID or -1 if no selection
 */
int MainWindow::getSelectedProcessPID_() const {
    const QModelIndexList selectedRows = m_processTableView->selectionModel()->selectedRows();
    if (selectedRows.isEmpty()) {
        return -1;
    }

    const QModelIndex index = selectedRows.first();
    if (!index.isValid()) {
        return -1;
    }

    return m_processModel->item(index.row(), TABLE_COLUMN_PID)->data(Qt::UserRole).toInt();
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