// SPDX-License-Identifier: GPL-3.0-only
#include "DeveloperPage.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QDesktopServices>
#include <QFileInfoList>
#include <QLabel>
#include <QLoggingCategory>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPainter>
#include <QPushButton>
#include <QSysInfo>
#include <QStatusBar>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QSplashScreen>
#include <QSvgRenderer>

#include "Application.h"
#include "BuildConfig.h"
#include "ui/MainWindow.h"
#include "settings/SettingsObject.h"

namespace {
QString logsPath()
{
    return QDir(APPLICATION->dataRoot()).filePath(QStringLiteral("logs"));
}

QString diagnosticsText()
{
    QStringList lines;
    lines << QStringLiteral("PollyMC-Continued diagnostics")
          << QStringLiteral("Version: %1").arg(BuildConfig.printableVersionString())
          << QStringLiteral("Commit: %1").arg(BuildConfig.GIT_COMMIT.isEmpty() ? QStringLiteral("unknown") : BuildConfig.GIT_COMMIT)
          << QStringLiteral("Build platform: %1").arg(BuildConfig.BUILD_PLATFORM.isEmpty() ? QStringLiteral("unknown") : BuildConfig.BUILD_PLATFORM)
          << QStringLiteral("Qt version: %1").arg(qVersion())
          << QStringLiteral("Operating system: %1 (%2)").arg(QSysInfo::prettyProductName(), QSysInfo::currentCpuArchitecture())
          << QStringLiteral("Portable mode: %1").arg(APPLICATION->isPortable() ? QStringLiteral("yes") : QStringLiteral("no"))
          << QStringLiteral("Logs directory: %1").arg(QDir::toNativeSeparators(logsPath()))
          << QStringLiteral("Generated: %1").arg(QDateTime::currentDateTime().toString(Qt::ISODate));

    QDir dir(logsPath());
    const QFileInfoList files = dir.entryInfoList(QDir::Files, QDir::Time);
    lines << QString() << QStringLiteral("Log files (names and sizes only):");
    if (files.isEmpty()) {
        lines << QStringLiteral("  No log files found.");
    } else {
        for (const QFileInfo& file : files)
            lines << QStringLiteral("  %1 — %2 bytes — modified %3")
                         .arg(file.fileName()).arg(file.size()).arg(file.lastModified().toString(Qt::ISODate));
    }
    return lines.join(QLatin1Char('\n'));
}

<<<<<<< HEAD
void showSplashPreview(QWidget* parent)
=======
void showSplashPreview()
>>>>>>> b8f275a (Fixed errors)
{
    QPixmap pixmap(400, 260);
    pixmap.fill(QColor(QStringLiteral("#191919")));
    QSvgRenderer renderer(QStringLiteral(":/branding/splash-logo.svg"));
    if (renderer.isValid()) {
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        renderer.render(&painter, QRectF(40, 76, 320, 107));
    }
<<<<<<< HEAD
    auto* splash = new QSplashScreen(parent, pixmap, Qt::WindowStaysOnTopHint);
=======
    auto* splash = new QSplashScreen(pixmap, Qt::WindowStaysOnTopHint);
>>>>>>> b8f275a (Fixed errors)
    splash->show();
    splash->raise();
    QTimer::singleShot(1800, splash, &QSplashScreen::close);
    QTimer::singleShot(1900, splash, &QObject::deleteLater);
}
} // namespace

DeveloperPage::DeveloperPage(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    auto* intro = new QLabel(tr("Developer tools are intended for testing and troubleshooting. Actions below change launcher preferences; they do not delete instances or accounts."), this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    auto* starToast = new QPushButton(tr("Show GitHub star reminder now"), this);
    starToast->setToolTip(tr("Display the reminder immediately, ignoring its normal schedule and dismissed state."));
    layout->addWidget(starToast);
    connect(starToast, &QPushButton::clicked, this, []() {
        auto* mainWindow = APPLICATION->showMainWindow();
        if (mainWindow) {
            mainWindow->triggerStarToastForDeveloper();
        }
    });

    auto* resetFirstRun = new QPushButton(tr("Reset first-run prompts"), this);
    resetFirstRun->setToolTip(tr("Make the language and Java setup prompts eligible to appear again."));
    layout->addWidget(resetFirstRun);
    connect(resetFirstRun, &QPushButton::clicked, this, [this]() {
        auto* settings = APPLICATION->settings();
        settings->set("Language", QString());
        settings->set("UserAskedAboutAutomaticJavaDownload", false);
        settings->set("IgnoreJavaWizard", false);
        settings->set("AppearanceMigrated", false);
        QMessageBox::information(this, tr("Developer tools"), tr("First-run preferences have been reset. Some prompts may appear after restarting PollyMC-Continued."));
    });

    auto* resetStar = new QPushButton(tr("Reset GitHub star reminder"), this);
    resetStar->setToolTip(tr("Allow the GitHub star reminder to be shown again."));
    layout->addWidget(resetStar);
    connect(resetStar, &QPushButton::clicked, this, []() {
        auto* settings = APPLICATION->settings();
        settings->set("StarReminderDismissed", false);
        settings->set("StarReminderLastShown", QString());
        QMessageBox::information(nullptr, QObject::tr("Developer tools"), QObject::tr("The star reminder schedule has been reset."));
    });

    auto* splash = new QPushButton(tr("Preview splash screen"), this);
    splash->setToolTip(tr("Show a temporary preview; this does not change startup behavior."));
    layout->addWidget(splash);
<<<<<<< HEAD
    connect(splash, &QPushButton::clicked, this, [this]() { showSplashPreview(this); });
=======
    connect(splash, &QPushButton::clicked, this, []() { showSplashPreview(); });
>>>>>>> b8f275a (Fixed errors)

    auto* openLogs = new QPushButton(tr("Open logs folder"), this);
    layout->addWidget(openLogs);
    connect(openLogs, &QPushButton::clicked, this, [this]() {
        const QString path = logsPath();
        if (!QDir().mkpath(path) || !QDesktopServices::openUrl(QUrl::fromLocalFile(path)))
            QMessageBox::warning(this, tr("Developer tools"), tr("Could not open the logs folder:\n%1").arg(path));
    });

    auto* copyDiagnostics = new QPushButton(tr("Copy diagnostic summary"), this);
    copyDiagnostics->setToolTip(tr("Copies version, platform, and log-file metadata only; log contents and account details are not included."));
    layout->addWidget(copyDiagnostics);
    connect(copyDiagnostics, &QPushButton::clicked, this, [this]() {
        QApplication::clipboard()->setText(diagnosticsText());
        QMessageBox::information(this, tr("Developer tools"), tr("Diagnostic summary copied to the clipboard. Review it before sharing because it includes the local logs-folder path."));
    });

    auto* verboseLogging = new QCheckBox(tr("Enable verbose Qt logging until restart"), this);
    verboseLogging->setToolTip(tr("Enables debug and informational Qt messages for this session. This may produce more output and can expose diagnostic details in logs."));
    layout->addWidget(verboseLogging);
    connect(verboseLogging, &QCheckBox::toggled, this, [](bool enabled) {
        if (enabled) {
            QLoggingCategory::setFilterRules(QStringLiteral("*.debug=true\n*.info=true"));
            qInfo() << "Developer Mode enabled verbose Qt logging for this session.";
        } else {
            // Empty rules restore Qt's default category filtering; restart to reload custom rules.
            QLoggingCategory::setFilterRules(QString());
            qInfo() << "Developer Mode disabled its verbose Qt logging override.";
        }
    });

    auto* updateCheck = new QPushButton(tr("Run real update check now"), this);
    updateCheck->setToolTip(tr("Runs PollyMC's normal update check. This contacts the configured update service."));
    layout->addWidget(updateCheck);
    connect(updateCheck, &QPushButton::clicked, this, [this]() {
        auto* mainWindow = APPLICATION->showMainWindow();
        if (!mainWindow) {
            QMessageBox::warning(this, tr("Developer tools"), tr("The main window is not available."));
            return;
        }
        if (!APPLICATION->updaterEnabled()) {
            QMessageBox::information(this, tr("Developer tools"), tr("The updater is not enabled in this build."));
            return;
        }
<<<<<<< HEAD
        mainWindow->checkForUpdates();
=======
        mainWindow->checkForUpdatesForDeveloper();
>>>>>>> b8f275a (Fixed errors)
    });

    auto* simulateFailure = new QPushButton(tr("Simulate update-check failure"), this);
    simulateFailure->setToolTip(tr("Shows a test-only failure message. It does not contact the updater or change update settings."));
    layout->addWidget(simulateFailure);
    connect(simulateFailure, &QPushButton::clicked, this, [this]() {
        QMessageBox::warning(this, tr("Simulated update failure"),
                             tr("This is a simulated network failure for UI testing. No update request was sent and no launcher files were changed."));
    });

    auto* slowNetwork = new QPushButton(tr("Simulate slow network (5 seconds)"), this);
    slowNetwork->setToolTip(tr("Simulates a delayed response locally; it does not throttle or modify real network traffic."));
    layout->addWidget(slowNetwork);
    connect(slowNetwork, &QPushButton::clicked, this, [this]() {
        auto* progress = new QProgressDialog(tr("Simulating a slow update response…"), tr("Cancel"), 0, 5, this);
        progress->setWindowTitle(tr("Slow-network test"));
        progress->setWindowModality(Qt::WindowModal);
        progress->setMinimumDuration(0);
        progress->setValue(0);
        auto* elapsed = new int(0);
        auto* timer = new QTimer(progress);
        connect(timer, &QTimer::timeout, progress, [this, progress, timer, elapsed]() {
            ++(*elapsed);
            progress->setValue(*elapsed);
            if (*elapsed >= 5) {
                timer->stop();
                progress->close();
                QMessageBox::information(this, tr("Slow-network test"), tr("The simulated response completed after five seconds. No network request was made."));
                delete elapsed;
                progress->deleteLater();
            }
        });
        connect(progress, &QProgressDialog::canceled, this, [timer, elapsed, progress]() {
            timer->stop();
            delete elapsed;
            progress->deleteLater();
        });
        timer->start(1000);
        progress->show();
    });

    auto* crashTest = new QPushButton(tr("Crash-test launcher (unsafe)"), this);
    crashTest->setToolTip(tr("Intentionally terminates the process. Unsaved launcher state may be lost."));
    layout->addWidget(crashTest);
    connect(crashTest, &QPushButton::clicked, this, [this]() {
        const auto answer = QMessageBox::warning(this, tr("Confirm crash test"),
            tr("This will immediately terminate PollyMC-Continued without a normal shutdown. Unsaved settings or UI state may be lost. Only continue when testing crash handling."),
            QMessageBox::Abort | QMessageBox::Yes, QMessageBox::Abort);
        if (answer == QMessageBox::Yes) {
            qCritical() << "Developer Mode: intentional crash test requested.";
            qFatal("PollyMC-Continued Developer Mode intentional crash test");
        }
    });

    auto* disable = new QPushButton(tr("Disable Developer Mode"), this);
    layout->addWidget(disable);
    connect(disable, &QPushButton::clicked, this, [this]() {
        APPLICATION->settings()->set("DeveloperMode", false);
        QMessageBox::information(this, tr("Developer tools"), tr("Developer Mode is disabled. The Developer page will disappear when Settings is reopened. The POLLYMC_DEV environment override, if set, must also be removed."));
    });
    layout->addStretch(1);
}

bool DeveloperPage::shouldDisplay() const
{
    return qEnvironmentVariableIsSet("POLLYMC_DEV") ||
           APPLICATION->settings()->get("DeveloperMode").toBool();
}
