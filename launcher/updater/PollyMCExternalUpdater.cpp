// SPDX-FileCopyrightText: 2023 Rachel Powers <508861+Ryex@users.noreply.github.com>
//
// SPDX-License-Identifier: GPL-3.0-only

/*
 *  PollyMC-Continued - Minecraft Launcher
 *  Copyright (C) 2023 Rachel Powers <508861+Ryex@users.noreply.github.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "PollyMCExternalUpdater.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QMessageBox>
#include <QProcess>
#include <QProgressDialog>
#include <QSettings>
#include <QTimer>
#include <algorithm>
#include <memory>

#include "StringUtils.h"
#include "UpdaterNames.h"

#include "Application.h"
#include "BuildConfig.h"

#include "ui/dialogs/UpdateAvailableDialog.h"

namespace {
// Give the launcher time to paint and settle before the first background check.
constexpr int startupCheckDelayMs = 5000;
}  // namespace

class PollyMCExternalUpdater::Private {
   public:
    QDir appDir;
    QDir dataDir;
    QTimer updateTimer;
    bool allowBeta{};
    bool autoCheck{};
    QString autoUpdateMode;
    // True while a 5-minute deferred retry (game was running) is armed.
    // Cleared when that retry runs; see checkForUpdates().
    bool deferredForGameRunning = false;
    double updateInterval{};
    QDateTime lastCheck;
    std::unique_ptr<QSettings> settings;

    QWidget* parent{};

    bool checkingUpdates{};

    // Async check state (the check runs in the background so the UI never blocks).
    QProcess* proc = nullptr;
    QTimer* killTimer = nullptr;
    QProgressDialog* progress = nullptr;
    bool checkTriggeredByUser = false;
    bool checkWasDeferredRetry = false;
    // True when the running check only exists to preview the latest release in the update dialog.
    bool checkIsPreview = false;
};

PollyMCExternalUpdater::PollyMCExternalUpdater(QWidget* parent, const QString& appDir, const QString& dataDir)
    : priv(new PollyMCExternalUpdater::Private())
{
    priv->appDir = QDir(appDir);
    priv->dataDir = QDir(dataDir);
    auto settings_file = UpdaterNames::migrateFile(priv->dataDir.absolutePath(), UpdaterNames::SettingsFile);
    priv->settings = std::make_unique<QSettings>(settings_file, QSettings::Format::IniFormat);
    priv->allowBeta = priv->settings->value("allow_beta", false).toBool();
    priv->autoUpdateMode = priv->settings->value("auto_update_mode", "Ask").toString();
    if (priv->autoUpdateMode != "Always" && priv->autoUpdateMode != "Ask" && priv->autoUpdateMode != "Never")
        priv->autoUpdateMode = "Ask";
    priv->autoCheck = priv->settings->value("auto_check", true).toBool();
    bool interval_ok = false;
    // default once per day
    priv->updateInterval = priv->settings->value("update_interval", 86400).toInt(&interval_ok);
    if (!interval_ok) {
        priv->updateInterval = 86400;
    }
    auto last_check = priv->settings->value("last_check");
    if (!last_check.isNull() && last_check.isValid()) {
        priv->lastCheck = QDateTime::fromString(last_check.toString(), Qt::ISODate);
    }
    priv->parent = parent;
    connectTimer();
    resetAutoCheckTimer();
    if (priv->updateInterval == 0) {  // "On Launch"
        // Never run the check from inside the constructor: it is created while the
        // main window is still being set up. Let startup finish first.
        QTimer::singleShot(startupCheckDelayMs, this, [this]() { checkForUpdates(false); });
    }
}

PollyMCExternalUpdater::~PollyMCExternalUpdater()
{
    if (priv->proc) {
        priv->proc->disconnect(this);
        priv->proc->kill();
        priv->proc->waitForFinished(1000);
        delete priv->proc;
        priv->proc = nullptr;
    }
    delete priv->progress;
    priv->progress = nullptr;
    if (priv->updateTimer.isActive()) {
        priv->updateTimer.stop();
    }
    disconnectTimer();
    priv->settings->sync();
    delete priv;
}

void PollyMCExternalUpdater::checkForUpdates()
{
    checkForUpdates(true);
}

bool PollyMCExternalUpdater::previewLatestRelease()
{
    checkForUpdates(true, true);
    return true;
}

void PollyMCExternalUpdater::checkForUpdates(bool triggeredByUser, bool preview)
{
    if (priv->checkingUpdates) {
        if (priv->updateTimer.isActive()) {
            priv->updateTimer.stop();
        }
        if (preview) {
            QMessageBox::information(priv->parent, tr("Update UI preview"), tr("An update check is already running. Try again in a moment."));
        }
        return;
    }
    priv->checkingUpdates = true;
    priv->checkIsPreview = preview;

    // Snapshot the pending-retry flag: a deferred retry arming later must
    // survive the end-of-check reschedule, while a completed retry must
    // restore the user's repeating schedule (see finishCheck()).
    priv->checkWasDeferredRetry = priv->deferredForGameRunning;
    priv->deferredForGameRunning = false;
    priv->checkTriggeredByUser = triggeredByUser;

    if (triggeredByUser) {
        priv->progress = new QProgressDialog(tr("Checking for updates..."), "", 0, 0, priv->parent);
        priv->progress->setCancelButton(nullptr);
        priv->progress->setAttribute(Qt::WA_DeleteOnClose, false);
        priv->progress->adjustSize();
        priv->progress->show();
    }

    auto exe_name = QStringLiteral("%1_updater").arg(BuildConfig.LAUNCHER_APP_BINARY_NAME);
    priv->proc = new QProcess(this);
#if defined Q_OS_WIN32
    exe_name.append(".exe");

    auto env = QProcessEnvironment::systemEnvironment();
    env.insert("__COMPAT_LAYER", "RUNASINVOKER");
    priv->proc->setProcessEnvironment(env);
#else
    exe_name = QString("bin/%1").arg(exe_name);
#endif

    QStringList args = { "--check-only", "--dir", priv->dataDir.absolutePath(), "--debug" };
    if (priv->allowBeta) {
        args.append("--pre-release");
    }
    if (preview) {
        // Ask the updater to report the latest release even when it is not newer than the installed one.
        args.append("--show-latest");
    }

    connect(priv->proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError err) {
        // Only a failure to start is handled here; crashes/timeouts are handled via finished().
        if (err != QProcess::FailedToStart || !priv->proc)
            return;
        const auto reason = priv->proc->errorString();
        qDebug() << "Failed to start updater. reason:" << err << reason;
        finishCheck(-1, {}, {}, tr("Failed to start the updater.\nReason: %1.").arg(reason));
    });
    connect(priv->proc, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus status) {
        if (!priv->proc)
            return;
        auto out = priv->proc->readAllStandardOutput();
        auto err = priv->proc->readAllStandardError();
        if (status == QProcess::CrashExit) {
            finishCheck(-1, out, err, tr("The updater crashed."));
            return;
        }
        finishCheck(exitCode, out, err, {});
    });

    // Safety net: never let a hung updater keep the check "running" forever.
    priv->killTimer = new QTimer(this);
    priv->killTimer->setSingleShot(true);
    connect(priv->killTimer, &QTimer::timeout, this, [this]() {
        if (!priv->proc)
            return;
        qDebug() << "Updater failed to close after 60 seconds.";
        priv->proc->disconnect(this);
        priv->proc->kill();
        priv->proc->waitForFinished(1000);
        finishCheck(-1, {}, priv->proc->readAll(), tr("The updater did not finish within 60 seconds."));
    });
    priv->killTimer->start(60000);

    priv->proc->start(priv->appDir.absoluteFilePath(exe_name), args);
}

void PollyMCExternalUpdater::finishCheck(int exit_code, const QByteArray& std_output, const QByteArray& std_error, const QString& failureReason)
{
    // Tear down the helper objects first so nothing below can re-enter this function.
    if (priv->killTimer) {
        priv->killTimer->stop();
        priv->killTimer->deleteLater();
        priv->killTimer = nullptr;
    }
    if (priv->proc) {
        priv->proc->disconnect(this);
        priv->proc->deleteLater();
        priv->proc = nullptr;
    }
    if (priv->progress) {
        priv->progress->hide();
        priv->progress->deleteLater();
        priv->progress = nullptr;
    }

    const bool triggeredByUser = priv->checkTriggeredByUser;
    const bool wasDeferredRetry = priv->checkWasDeferredRetry;
    const bool preview = priv->checkIsPreview;
    priv->checkIsPreview = false;

    if (!failureReason.isEmpty()) {
        // Background (automatic) checks fail silently: a missing network or updater must
        // never greet the user with a dialog at startup. Manual checks still tell them.
        qDebug() << "Update check failed:" << failureReason << qPrintable(QString::fromUtf8(std_error));
        if (triggeredByUser) {
            auto msgBox = QMessageBox(QMessageBox::Information, tr("Update Check Failed"), failureReason, QMessageBox::Ok, priv->parent);
            msgBox.setDetailedText(QString::fromUtf8(std_error));
            msgBox.setMinimumWidth(460);
            msgBox.adjustSize();
            msgBox.exec();
        }
    } else {
        switch (exit_code) {
            case 0:
                // no update available
                if (preview) {
                    auto msgBox = QMessageBox(QMessageBox::Information, tr("Update UI preview"),
                                              tr("The updater did not report any release to show."), QMessageBox::Ok, priv->parent);
                    msgBox.setMinimumWidth(460);
                    msgBox.adjustSize();
                    msgBox.exec();
                } else if (triggeredByUser) {
                    qDebug() << "No update available";
                    auto msgBox = QMessageBox(QMessageBox::Information, tr("No Update Available"), tr("You are running the latest version."),
                                              QMessageBox::Ok, priv->parent);
                    msgBox.setMinimumWidth(460);
                    msgBox.adjustSize();
                    msgBox.exec();
                }
                break;
            case 1:
                // there was an error
                qDebug() << "Updater subprocess error" << qPrintable(QString::fromUtf8(std_error));
                if (triggeredByUser) {
                    auto msgBox = QMessageBox(QMessageBox::Warning, tr("Update Check Error"),
                                              tr("There was an error running the update check."), QMessageBox::Ok, priv->parent);
                    msgBox.setDetailedText(QString::fromUtf8(std_error));
                    msgBox.setMinimumWidth(460);
                    msgBox.adjustSize();
                    msgBox.exec();
                }
                break;
            case 100:
                // update available
                {
                    auto [first_line, remainder1] = StringUtils::splitFirst(std_output, '\n');
                    auto [second_line, remainder2] = StringUtils::splitFirst(remainder1, '\n');
                    auto [third_line, release_notes] = StringUtils::splitFirst(remainder2, '\n');
                    auto version_name = StringUtils::splitFirst(first_line, ": ").second.trimmed();
                    auto version_tag = StringUtils::splitFirst(second_line, ": ").second.trimmed();
                    auto release_timestamp = QDateTime::fromString(StringUtils::splitFirst(third_line, ": ").second.trimmed(), Qt::ISODate);
                    qDebug() << "Update available:" << version_name << version_tag << release_timestamp;
                    qDebug() << "Update release notes:" << release_notes;

                    offerUpdate(version_name, version_tag, release_notes, triggeredByUser, preview);
                }
                break;
            default:
                // unknown error code
                qDebug() << "Updater exited with unknown code" << exit_code;
                if (triggeredByUser) {
                    auto msgBox = QMessageBox(QMessageBox::Information, tr("Unknown Update Error"),
                                              tr("The updater exited with an unknown condition.\nExit Code: %1").arg(QString::number(exit_code)),
                                              QMessageBox::Ok, priv->parent);
                    auto detail_txt = tr("StdOut: %1\nStdErr: %2").arg(QString::fromUtf8(std_output)).arg(QString::fromUtf8(std_error));
                    msgBox.setDetailedText(detail_txt);
                    msgBox.setMinimumWidth(460);
                    msgBox.adjustSize();
                    msgBox.exec();
                }
        }
    }

    if (!preview) {
        // A UI preview is not a real check: do not move the automatic-check schedule.
        priv->lastCheck = QDateTime::currentDateTime();
        priv->settings->setValue("last_check", priv->lastCheck.toString(Qt::ISODate));
        priv->settings->sync();
    }
    if (priv->deferredForGameRunning) {
        // This check deferred to the armed 5-minute retry; leave
        // that timer alone instead of rescheduling the full interval.
    } else {
        resetAutoCheckTimer();
        if (wasDeferredRetry) {
            // A deferred retry just completed without re-deferring: undo
            // the one-shot setup so the user's repeating schedule resumes.
            priv->updateTimer.setSingleShot(false);
        }
    }
    priv->checkingUpdates = false;
}

bool PollyMCExternalUpdater::getAutomaticallyChecksForUpdates()
{
    return priv->autoCheck;
}

double PollyMCExternalUpdater::getUpdateCheckInterval()
{
    return priv->updateInterval;
}

bool PollyMCExternalUpdater::getBetaAllowed()
{
    return priv->allowBeta;
}

void PollyMCExternalUpdater::setAutomaticallyChecksForUpdates(bool check)
{
    priv->autoCheck = check;
    priv->settings->setValue("auto_check", check);
    priv->settings->sync();
    resetAutoCheckTimer();
}

void PollyMCExternalUpdater::setUpdateCheckInterval(double seconds)
{
    priv->updateInterval = seconds;
    priv->settings->setValue("update_interval", seconds);
    priv->settings->sync();
    resetAutoCheckTimer();
}

void PollyMCExternalUpdater::setBetaAllowed(bool allowed)
{
    priv->allowBeta = allowed;
    priv->settings->setValue("auto_beta", allowed);
    priv->settings->sync();
}

QString PollyMCExternalUpdater::getAutoUpdateMode()
{
    return priv->autoUpdateMode;
}

void PollyMCExternalUpdater::setAutoUpdateMode(const QString& mode)
{
    if (mode != "Always" && mode != "Ask" && mode != "Never")
        return;
    priv->autoUpdateMode = mode;
    priv->settings->setValue("auto_update_mode", mode);
    priv->settings->sync();
}

void PollyMCExternalUpdater::resetAutoCheckTimer()
{
    if (priv->autoCheck && priv->updateInterval > 0) {
        auto now = QDateTime::currentDateTime();

        qint64 timeoutMs = 0;

        if (priv->lastCheck.isValid()) {
            qint64 diff = priv->lastCheck.secsTo(now);
            qint64 secs_left = std::max<qint64>(priv->updateInterval - diff, 0);
            timeoutMs = secs_left * 1000;
        }

        // Never fire right at startup (always the case on a first run, where there is no
        // last check yet): let the launcher finish painting before touching the network.
        timeoutMs = std::max<qint64>(timeoutMs, startupCheckDelayMs);
        timeoutMs = std::min(timeoutMs, static_cast<qint64>(INT_MAX));

        qDebug() << "Auto update timer starting," << timeoutMs / 1000 << "seconds left";
        priv->updateTimer.start(static_cast<int>(timeoutMs));
    } else {
        if (priv->updateTimer.isActive()) {
            priv->updateTimer.stop();
        }
    }
}

void PollyMCExternalUpdater::connectTimer()
{
    connect(&priv->updateTimer, &QTimer::timeout, this, &PollyMCExternalUpdater::autoCheckTimerFired);
}

void PollyMCExternalUpdater::disconnectTimer()
{
    disconnect(&priv->updateTimer, &QTimer::timeout, this, &PollyMCExternalUpdater::autoCheckTimerFired);
}

void PollyMCExternalUpdater::armDeferredRetry()
{
    priv->deferredForGameRunning = true;
    // Single-shot: exactly one retry. The flag is consumed at the top of
    // the next check; a completed retry restores the repeating schedule
    // at the tail of checkForUpdates().
    priv->updateTimer.setSingleShot(true);
    priv->updateTimer.setInterval(5 * 60 * 1000);
    priv->updateTimer.start();
}

void PollyMCExternalUpdater::autoCheckTimerFired()
{
    qDebug() << "Auto update Timer fired";
    if (priv->autoUpdateMode == "Never")
        return;
    checkForUpdates(false);
}

void PollyMCExternalUpdater::offerUpdate(const QString& version_name, const QString& version_tag,
                                       const QString& release_notes, bool triggeredByUser, bool preview)
{
    // Developer preview: always show the regular dialog (ignoring skipped versions, the auto-update
    // mode and running games), but never install or record a skip.
    if (preview) {
        UpdateAvailableDialog dlg(BuildConfig.printableVersionString(), version_name, release_notes, priv->parent, true);
        dlg.exec();
        return;
    }
    // Silent path: auto check, mode Always. No UI at all (not even the
    // per-version skip box); defer while a game runs, else install now.
    if (!triggeredByUser && priv->autoUpdateMode == "Always") {
        if (!APPLICATION->updatesAreAllowed()) {
            qDebug() << "Auto-update deferred: a game is running";
            armDeferredRetry();
            return;
        }
        performUpdate(version_tag, true);
        return;
    }
    // Never auto-prompt mid-game either: defer the dialog the same way.
    if (!triggeredByUser && !APPLICATION->updatesAreAllowed()) {
        qDebug() << "Update prompt deferred: a game is running";
        armDeferredRetry();
        return;
    }
    priv->settings->beginGroup("skip");
    auto should_skip = priv->settings->value(version_tag, false).toBool();
    priv->settings->endGroup();

    if (should_skip) {
        auto msgBox = QMessageBox(QMessageBox::Information, tr("No Update Available"), tr("There are no new updates available."),
                                  QMessageBox::Ok, priv->parent);
        msgBox.setMinimumWidth(460);
        msgBox.adjustSize();
        msgBox.exec();
        return;
    }

    UpdateAvailableDialog dlg(BuildConfig.printableVersionString(), version_name, release_notes);

    auto result = dlg.exec();
    qDebug() << "offer dlg result" << result;
    switch (result) {
        case UpdateAvailableDialog::Install: {
            performUpdate(version_tag, false);
            return;
        }
        case UpdateAvailableDialog::Skip: {
            priv->settings->beginGroup("skip");
            priv->settings->setValue(version_tag, true);
            priv->settings->endGroup();
            priv->settings->sync();
            return;
        }
        default: {
            return;
        }
    }
}

void PollyMCExternalUpdater::performUpdate(const QString& version_tag, bool silent)
{
    QProcess proc;
    auto exe_name = QStringLiteral("%1_updater").arg(BuildConfig.LAUNCHER_APP_BINARY_NAME);
#if defined Q_OS_WIN32
    exe_name.append(".exe");

    auto env = QProcessEnvironment::systemEnvironment();
    env.insert("__COMPAT_LAYER", "RUNASINVOKER");
    proc.setProcessEnvironment(env);
#else
    exe_name = QString("bin/%1").arg(exe_name);
#endif

    QStringList args = { "--dir", priv->dataDir.absolutePath(), "--install-version", version_tag };
    if (priv->allowBeta) {
        args.append("--pre-release");
    }
    if (silent) {
        args.append("--silent");
    }

    proc.setProgram(priv->appDir.absoluteFilePath(exe_name));
    proc.setArguments(args);
    auto result = proc.startDetached();
    if (!result) {
        qDebug() << "Failed to start updater:" << proc.error() << proc.errorString();
    }
    QCoreApplication::exit();
}
