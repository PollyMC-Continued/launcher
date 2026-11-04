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

#include <QDateTime>
#include <QString>

class QObject;
class QTimer;

/*!
 * \brief A small JSON "crash.marker" file in the data path that survives everything except a
 * clean shutdown, plus a cheap heartbeat that keeps its timestamp fresh.
 *
 * The marker is written once startup has come far enough, refreshed every ~30 seconds and
 * removed on every clean exit path: a normal quit (aboutToQuit) and the OS ending the session
 * (commitDataRequest / saveStateRequest, plus WM_QUERYENDSESSION / WM_ENDSESSION and console
 * control events on Windows). A power loss, reboot or logoff while the launcher is open
 * therefore no longer looks like a crash on the next start.
 *
 * On startup, classifyPreviousRun() looks at the leftover marker and decides how the previous
 * run ended:
 *  - Clean          no marker: the previous run shut down cleanly.
 *  - SystemShutdown the machine was rebooted or lost power (the boot identity changed) - not a crash.
 *  - SoftExit       the marker is old, unreadable or pre-JSON, and the previous session's log
 *                   shows no crash evidence - not worth asking the user about.
 *  - Crash          same boot, and either the previous log has crash evidence or the process
 *                   disappeared without a clean shutdown while the machine stayed up.
 */
class CrashMarker {
   public:
    enum class PreviousRun { Clean, SystemShutdown, SoftExit, Crash };

    /// \param path       location of the marker file.
    /// \param startedAt  process start time, recorded in the marker.
    /// \param parent     owns the heartbeat timer (and thereby this object's timer); may be null.
    CrashMarker(QString path, QDateTime startedAt, QObject* parent);

    /// Reads the marker left behind by the previous run and classifies how that run ended.
    /// \param previousSessionLog  the previous session's log (already rotated to the "-1" file),
    ///                            searched for crash evidence. May be empty.
    PreviousRun classifyPreviousRun(const QString& previousSessionLog) const;

    /// (Re)writes the marker: launcher version, start time, heartbeat, boot identity.
    /// Cheap (a few hundred bytes, written atomically), so it is safe on the UI thread.
    void touch();

    /// Starts refreshing the heartbeat timestamp every ~30 seconds.
    void startHeartbeat();

    /// Deletes the marker and stops the heartbeat. Every clean exit path calls this; calling it
    /// more than once is fine.
    void remove();

    /// Deletes the marker but keeps the heartbeat armed: used for session-state notifications,
    /// which also arrive for plain session checkpoints and not only when the OS logs us off. If
    /// the session really is ending, the process dies long before the heartbeat re-arms the
    /// marker; if it was just a checkpoint, the marker is back within half a minute.
    void removeForStateSave();

    /// Deletes the marker file at \a path, from any thread. Used by the Windows console control
    /// handler, which runs on a thread of its own while the GUI thread is on its way out.
    static void removeAt(const QString& path);

    /// Reads at most \a maxBytes from the end of a text file (as UTF-8). Empty when the file is
    /// missing or unreadable - callers treat that as "no previous log".
    static QString readFileTail(const QString& path, qint64 maxBytes);

    /// Something that changes when the machine is rebooted or loses power, and stays the same
    /// while it merely sleeps or a user logs off. Empty when the platform has no cheap source
    /// for it, in which case the other classification signals have to decide.
    static QString bootIdentity();

    const QString& path() const { return m_path; }

   private:
    void writeMarker(const QDateTime& lastHeartbeat);
    void heartbeat();

    QString m_path;
    QDateTime m_startedAt;
    QTimer* m_heartbeatTimer = nullptr;  // parented to the QObject passed in, never deleted here
    QDateTime m_removedAt;               // invalid while the marker is (supposed to be) present
    bool m_writeFailed = false;          // warn about a failing data path only once
};
