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

class BugReportStatusChecker;
class QLabel;
class QPushButton;
class QTreeWidget;

/// Lists the bug reports sent from this launcher and what the team answered.
class MyReportsDialog : public QDialog {
    Q_OBJECT

   public:
    explicit MyReportsDialog(QWidget* parent = nullptr);

   private:
    void reload();
    void refresh();

    QTreeWidget* m_tree = nullptr;
    QLabel* m_empty = nullptr;
    QPushButton* m_refresh = nullptr;
    BugReportStatusChecker* m_checker = nullptr;
};
