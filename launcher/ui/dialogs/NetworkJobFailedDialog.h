// SPDX-License-Identifier: GPL-3.0-only
/*
 *  PollyMC-Continued - Minecraft Launcher
 *  Copyright (C) 2025 Octol1ttle <l1ttleofficial@outlook.com>
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
 */

#pragma once

#include <QDialog>
#include <QHash>
#include <QNetworkReply>

QT_BEGIN_NAMESPACE
namespace Ui {
class NetworkJobFailedDialog;
}
QT_END_NAMESPACE

class QLabel;
class QPushButton;
class QToolButton;

class NetworkJobFailedDialog : public QDialog {
    Q_OBJECT

   public:
    explicit NetworkJobFailedDialog(const QString& jobName, int attempts, int requests, int failed, QWidget* parent = nullptr);
    ~NetworkJobFailedDialog() override;

    /// \param error     the failure text shown in the details list (sink reason or reply error string).
    /// \param errorCode the reply's network error, when there was one; used to tell connection
    ///                  problems (timeouts, unreachable hosts) apart from launcher-side trouble.
    void addFailedRequest(const QUrl& url, QString error, QNetworkReply::NetworkError errorCode = QNetworkReply::NoError);

    /// Decides whether the whole job looks like "can't reach <host>" (at least 80% of the failed
    /// requests are timeouts or connection errors to one host) and switches the dialog to the
    /// friendly layout. Must be called after the last addFailedRequest(), before open().
    void finalizeFailedRequests();

   private slots:
    void copyUrl() const;
    void reportBug();
    /// Antivirus workaround: pick another folder for temporary downloads and retry the job.
    void retryWithDifferentTempFolder();
    /// Opens the guide on excluding the launcher from antivirus scanning.
    void openAntivirusGuide();
    /// Toggles the full URL/error list behind the "Show details" expander.
    void toggleDetails();

   private:
    /// Shows the antivirus hint label and the repair buttons (once a "Permission denied" is seen).
    void showPermissionRepairOptions() const;
    /// Rewrites the dialog for the connection-problem case: friendly text, details collapsed,
    /// report button out of sight.
    void showConnectionTroubleLayout(const QString& host);

    Ui::NetworkJobFailedDialog* m_ui;
    QString m_jobName;
    QLabel* m_permissionHint = nullptr;
    QPushButton* m_stagingButton = nullptr;
    QPushButton* m_avGuideButton = nullptr;
    QPushButton* m_reportButton = nullptr;
    QToolButton* m_detailsToggle = nullptr;
    bool m_permissionTrouble = false;

    // connection-trouble detection, filled by addFailedRequest
    int m_totalFailures = 0;
    int m_connectionFailures = 0;
    QHash<QString, int> m_failuresPerHost;
    bool m_friendlyLayout = false;
};
