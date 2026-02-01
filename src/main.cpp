#include <QApplication>
#include <QStyleFactory>
#include <QDir>
#include <QStandardPaths>
#include <QMessageBox>
#include <QIcon>
#include <QTranslator>
#include <QLocale>
#include <QDebug>

#include "mainwindow.h"

/**
 * @brief Main application entry point
 *
 * Initializes the Qt application, sets up the main window,
 * and starts the event loop.
 */
int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    // Set application properties
    app.setApplicationName("LuminaTask");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("dawillygene");
    app.setOrganizationDomain("github.com/dawillygene");

    // Set application icon (if available)
    if (QFile::exists(":/icons/app.png")) {
        app.setWindowIcon(QIcon(":/icons/app.png"));
    }

    // Set a modern style
    app.setStyle(QStyleFactory::create("Fusion"));

    // Dark theme palette (optional)
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::WindowText, Qt::white);
    darkPalette.setColor(QPalette::Base, QColor(25, 25, 25));
    darkPalette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
    darkPalette.setColor(QPalette::ToolTipText, Qt::black);
    darkPalette.setColor(QPalette::Text, Qt::white);
    darkPalette.setColor(QPalette::Button, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ButtonText, Qt::white);
    darkPalette.setColor(QPalette::BrightText, Qt::red);
    darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::HighlightedText, Qt::black);

    // Uncomment to enable dark theme
    // app.setPalette(darkPalette);

    // Check if running as root (optional warning)
    if (geteuid() == 0) {
        QMessageBox::warning(nullptr, "Root Warning",
            "Running LuminaTask as root may allow killing system processes. "
            "Use with caution!");
    }

    // Create and show main window
    MainWindow window;
    window.show();

    // Start the application event loop
    return app.exec();
}