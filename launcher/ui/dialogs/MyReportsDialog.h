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
#include <QSet>
#include <QString>

class BugReportStatusChecker;
class QLabel;
class QPushButton;
class QVBoxLayout;
class QWidget;

/// Lists the bug reports sent from this launcher and what the team answered, one card per report, newest first.
class MyReportsDialog : public QDialog {
    Q_OBJECT

   public:
    explicit MyReportsDialog(QWidget* parent = nullptr);

   private:
    void reload();
    void refresh();

    QVBoxLayout* m_cards = nullptr;    // the cards (inside the scroll area)
    QWidget* m_listPage = nullptr;     // scroll area with the cards
    QWidget* m_emptyPage = nullptr;    // shown when no report was sent yet
    QLabel* m_checked = nullptr;       // "Last checked: ..."
    QPushButton* m_refresh = nullptr;
    BugReportStatusChecker* m_checker = nullptr;
    QSet<QString> m_newIds;            // reports whose status the user had not seen when the window opened
};
