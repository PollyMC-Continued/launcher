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

#pragma once

#include <QObject>

#include "ExternalUpdater.h"

class QWidget;

/*!
 * An implementation for the updater on windows and linux that uses out external updater.
 */

class PollyMCExternalUpdater : public ExternalUpdater {
    Q_OBJECT

   public:
    PollyMCExternalUpdater(QWidget* parent, const QString& appDir, const QString& dataDir);
    ~PollyMCExternalUpdater() override;

    /*!
     * Check for updates manually, showing the user a progress bar and an alert if no updates are found.
     */
    void checkForUpdates() override;
    void checkForUpdates(bool triggeredByUser, bool preview = false);

    /*!
     * Shows the regular update dialog for the latest release (preview only, cannot install).
     */
    bool previewLatestRelease() override;

    /*!
     * Indicates whether or not to check for updates automatically.
     */
    bool getAutomaticallyChecksForUpdates() override;

    /*!
     * Indicates the current automatic update check interval in seconds.
     */
    double getUpdateCheckInterval() override;

    /*!
     * Indicates whether or not beta updates should be checked for in addition to regular releases.
     */
    bool getBetaAllowed() override;

    /*!
     * Set whether or not to check for updates automatically.
     *
     * The update schedule cycle will be reset in a short delay after the property’s new value is set. This is to allow
     * reverting this property without kicking off a schedule change immediately."
     */
    void setAutomaticallyChecksForUpdates(bool check) override;

    /*!
     * Set the current automatic update check interval in seconds.
     *
     * The update schedule cycle will be reset in a short delay after the property’s new value is set. This is to allow
     * reverting this property without kicking off a schedule change immediately."
     */
    void setUpdateCheckInterval(double seconds) override;

    /*!
     * Set whether or not beta updates should be checked for in addition to regular releases.
     */
    void setBetaAllowed(bool allowed) override;

    QString getAutoUpdateMode();  // "Always" | "Ask" | "Never"
    void setAutoUpdateMode(const QString& mode);

    void resetAutoCheckTimer();
    void disconnectTimer();
    void connectTimer();

    // Arms a one-shot 5-minute retry after deferring for a running game.
    void armDeferredRetry();
    void offerUpdate(const QString& version_name, const QString& version_tag, const QString& release_notes,
                     bool triggeredByUser = true, bool preview = false);
    void performUpdate(const QString& version_tag, bool silent = false);

   public slots:
    void autoCheckTimerFired();

   private:
    void finishCheck(int exit_code, const QByteArray& std_output, const QByteArray& std_error, const QString& failureReason);

    // A live parent for dialogs, or nullptr once the main window is gone (never dangling).
    QWidget* safeParent() const;
    // Collapses a failure reason into one short line for the status bar.
    static QString singleLine(const QString& text);
    // Quiet, non-blocking failure notice for background checks (status bar of the main window).
    void quietNotify(const QString& message);

    class Private;

    Private* priv;
};
