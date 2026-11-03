// SPDX-License-Identifier: GPL-3.0-only
/*
 *  PollyMC-Continued - Minecraft Launcher
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
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QList>
#include <QStringList>

#include "ProblemProvider.h"

class QLabel;
class QListWidget;
class QProgressBar;
class QPushButton;
class QTimer;
class MinecraftInstance;

/*
 * "Check instance" - looks for the things that most often stop a launch before you launch:
 * missing Java, corrupt mod jars, mods that conflict with each other, a broken/mismatched loader version,
 * too little disk space and a memory setting the computer cannot satisfy.
 *
 * The quick checks run when the dialog opens; scanning the mod jars and the mod list can take a moment on
 * big modpacks, so those run in the background and the result list fills in when they are done.
 */
class InstanceHealthCheckDialog : public QDialog {
    Q_OBJECT

   public:
    struct Issue {
        ProblemSeverity severity = ProblemSeverity::None;
        QString title;
        QString detail;
    };

    explicit InstanceHealthCheckDialog(MinecraftInstance* instance, QWidget* parent = nullptr);

   private slots:
    void startChecks();
    void onJarScanFinished();
    void pollModList();

   private:
    void runQuickChecks();
    void runModListChecks();
    void finishIfDone();
    void showResults();
    QString reportText() const;

    MinecraftInstance* m_instance;
    QList<Issue> m_issues;

    QLabel* m_summaryLabel = nullptr;
    QProgressBar* m_busyBar = nullptr;
    QListWidget* m_issuesList = nullptr;
    QPushButton* m_rerunButton = nullptr;
    QPushButton* m_modsFolderButton = nullptr;
    QPushButton* m_copyButton = nullptr;
    QPushButton* m_closeButton = nullptr;

    QFutureWatcher<QStringList>* m_jarWatcher = nullptr;
    QTimer* m_pollTimer = nullptr;
    QElapsedTimer m_waited;
    bool m_jarsDone = false;
    bool m_modListDone = false;
    bool m_modListUpdated = false;
};
