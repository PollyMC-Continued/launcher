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

QT_BEGIN_NAMESPACE
namespace Ui {
class NetworkJobFailedDialog;
}
QT_END_NAMESPACE

class QLabel;
class QPushButton;

class NetworkJobFailedDialog : public QDialog {
    Q_OBJECT

   public:
    explicit NetworkJobFailedDialog(const QString& jobName, int attempts, int requests, int failed, QWidget* parent = nullptr);
    ~NetworkJobFailedDialog() override;

    void addFailedRequest(const QUrl& url, QString error) const;

   private slots:
    void copyUrl() const;
    void reportBug();
    /// Antivirus workaround: pick another folder for temporary downloads and retry the job.
    void retryWithDifferentTempFolder();
    /// Opens the guide on excluding the launcher from antivirus scanning.
    void openAntivirusGuide();

   private:
    /// Shows the antivirus hint label and the repair buttons (once a "Permission denied" is seen).
    void showPermissionRepairOptions() const;

    Ui::NetworkJobFailedDialog* m_ui;
    QString m_jobName;
    QLabel* m_permissionHint = nullptr;
    QPushButton* m_stagingButton = nullptr;
    QPushButton* m_avGuideButton = nullptr;
    mutable bool m_permissionTrouble = false;
};
