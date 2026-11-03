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
#include <QString>

/**
 * "What's New" window: shown once after the launcher was updated, with the release notes of the
 * version that is now running. It can be reopened any time from Help > What's New.
 *
 * The notes come from the file the updater leaves behind when it installs a release (the GitHub
 * release body). The launcher keeps a copy of the last notes it showed, so the Help entry works offline.
 */
class WhatsNewDialog : public QDialog {
    Q_OBJECT
   public:
    /// `previousVersion` may be empty (e.g. when reopened from the Help menu).
    WhatsNewDialog(const QString& version, const QString& previousVersion, const QString& notesMarkdown, QWidget* parent = nullptr);

    struct Notes {
        QString version;
        QString markdown;
    };

    /// Reads and deletes the release notes the updater left for the freshly installed version.
    /// Returns an empty string if there are none.
    static QString takePendingNotes();
    /// Deletes leftover notes (from an update that never happened) without reading them.
    static void discardPendingNotes();

    /// Keeps the notes of `version` for the Help menu.
    static void storeNotes(const QString& version, const QString& markdown);
    /// The notes kept by storeNotes(); both fields are empty if there are none.
    static Notes loadStoredNotes();

    static QString releasesUrl();
};
