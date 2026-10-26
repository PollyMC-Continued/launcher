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
#include <QList>
#include <QString>

class QLabel;
class QListWidget;
class QTextBrowser;

/**
 * "What's New" browser: the successor of the old News window. The sidebar lists every version
 * recorded in the changelog bundled with the launcher; selecting one shows its release notes.
 *
 * Opened from the What's New toolbar button and from Help > What's New. The version currently
 * running is selected first when the list contains it, the newest release otherwise.
 */
class WhatsNewBrowserDialog : public QDialog {
    Q_OBJECT
   public:
    explicit WhatsNewBrowserDialog(QWidget* parent = nullptr);

    struct Release {
        QString version;   ///< the heading as written in the changelog, e.g. "v9.5.0"
        QString markdown;  ///< the section body, without the heading
    };

    /// Versions and notes parsed from the changelog bundled with the launcher
    /// (:/documents/CHANGELOG.md), newest first like in the file. Empty when it cannot be read.
    static QList<Release> loadReleases();

   private:
    void showRelease(const Release& release);

    QList<Release> m_releases;
    QListWidget* m_versionList = nullptr;
    QLabel* m_titleLabel = nullptr;
    QTextBrowser* m_notesBrowser = nullptr;
};
