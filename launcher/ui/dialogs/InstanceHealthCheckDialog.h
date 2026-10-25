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

#include "ProblemProvider.h"

class InstanceWindow;
class QLabel;
class QListWidget;
class QPushButton;
class MinecraftInstance;

/*
 * "Check instance" - looks for the things that most often stop a launch before you launch:
 * missing Java, corrupt mod jars and a broken/mismatched loader version.
 * The checks are quick and run synchronously when the dialog opens.
 */
class InstanceHealthCheckDialog : public QDialog {
    Q_OBJECT

   public:
    explicit InstanceHealthCheckDialog(MinecraftInstance* instance, QWidget* parent = nullptr);

   private:
    void runChecks();

    MinecraftInstance* m_instance;
    QLabel* m_summaryLabel = nullptr;
    QListWidget* m_issuesList = nullptr;
    QPushButton* m_closeButton = nullptr;
};
