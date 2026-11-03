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

#include "CrashMarker.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

#include <algorithm>

#include "BuildConfig.h"
#include "Exception.h"
#include "FileSystem.h"

#if defined(Q_OS_WIN)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(Q_OS_MACOS)
#include <sys/sysctl.h>
#include <sys/time.h>
#endif

namespace {
// The heartbeat interval: the marker's timestamp is allowed to be this far behind reality.
constexpr int HeartbeatIntervalMs = 30 * 1000;

// A leftover marker whose last heartbeat is older than this is stale: the previous run ended
// a long time ago and nobody remembers whether it crashed. Asking about it now is just noise.
constexpr qint64 StaleMarkerSeconds = 3600;

// A removeForStateSave() that we survive for this long was a session checkpoint, not the OS
// ending the session: re-arm the marker so real crashes are still detected afterwards.
constexpr qint64 StateSaveReArmSeconds = 120;

// Only this tail of the previous session's log is searched for crash evidence: whatever killed
// the process logged itself (or nothing at all) right at the end.
constexpr qsizetype CrashEvidenceTailChars = 16 * 1024;

// Lowercased: matched against a lowercased log with QString::contains.
bool logTailHasCrashEvidence(const QString& log)
{
    const QString tail = log.right(qMin(log.size(), CrashEvidenceTailChars)).toLower();
    if (tail.isEmpty()) {
        return false;
    }
    static const QStringList patterns = {
        // Unix signals and what Qt writes on its way down
        QStringLiteral("sigsegv"),   QStringLiteral("sigabrt"),  QStringLiteral("sigill"),
        QStringLiteral("sigfpe"),    QStringLiteral("sigbus"),
        // Windows structured exceptions
        QStringLiteral("access violation"), QStringLiteral("unhandled exception"), QStringLiteral("stack overflow"),
        // Qt's own fatal and abort paths ("%{if-fatal}Fatal:%{endif}" in the log pattern)
        QStringLiteral("fatal"),     QStringLiteral("assert"),   QStringLiteral("pure virtual"),
    };
    return std::any_of(patterns.cbegin(), patterns.cend(), [&tail](const QString& pattern) { return tail.contains(pattern); });
}
}  // namespace

CrashMarker::CrashMarker(QString path, QDateTime startedAt, QObject* parent)
    : m_path(std::move(path)), m_startedAt(std::move(startedAt))
{
    if (parent) {
        m_heartbeatTimer = new QTimer(parent);
        m_heartbeatTimer->setInterval(HeartbeatIntervalMs);
        // coarse is plenty accurate for a staleness timestamp and wakes the CPU less
        m_heartbeatTimer->setTimerType(Qt::CoarseTimer);
        QObject::connect(m_heartbeatTimer, &QTimer::timeout, m_heartbeatTimer, [this]() { heartbeat(); });
    }
}

CrashMarker::PreviousRun CrashMarker::classifyPreviousRun(const QString& previousSessionLog) const
{
    QFile marker(m_path);
    if (!marker.exists()) {
        return PreviousRun::Clean;
    }
    if (!marker.open(QIODevice::ReadOnly)) {
        // unreadable rather than crashed: do not bother the user about it
        return PreviousRun::SoftExit;
    }

    const auto doc = QJsonDocument::fromJson(marker.readAll());
    marker.close();
    if (!doc.isObject()) {
        // Markers written by older versions hold a plain timestamp (or nothing at all), and a
        // corrupt marker is no evidence of a crash either: treat both as a soft exit.
        return PreviousRun::SoftExit;
    }
    const QJsonObject obj = doc.object();

    // The machine restarted or lost power while the launcher was open: the launcher never had a
    // chance to clean up, but nothing crashed either. This check comes first on purpose - a
    // reboot explains the leftover marker even if the old log happens to contain scary words.
    const QString markerBootId = obj.value(QStringLiteral("bootId")).toString();
    const QString currentBootId = bootIdentity();
    if (!markerBootId.isEmpty() && !currentBootId.isEmpty() && markerBootId != currentBootId) {
        return PreviousRun::SystemShutdown;
    }

    if (logTailHasCrashEvidence(previousSessionLog)) {
        return PreviousRun::Crash;
    }

    // No crash evidence in the log: a *recent* unclean exit still means the process disappeared
    // while the machine stayed up (kill, OOM, hard freeze) and is worth reporting. A stale
    // marker does not tell us anything anymore.
    const QDateTime lastHeartbeat = QDateTime::fromString(obj.value(QStringLiteral("lastHeartbeat")).toString(), Qt::ISODate);
    if (!lastHeartbeat.isValid() || lastHeartbeat.secsTo(QDateTime::currentDateTimeUtc()) > StaleMarkerSeconds) {
        return PreviousRun::SoftExit;
    }
    return PreviousRun::Crash;
}

void CrashMarker::touch()
{
    m_removedAt = QDateTime();
    writeMarker(QDateTime::currentDateTimeUtc());
}

void CrashMarker::startHeartbeat()
{
    if (m_heartbeatTimer && !m_heartbeatTimer->isActive()) {
        m_heartbeatTimer->start();
    }
}

void CrashMarker::remove()
{
    if (m_heartbeatTimer) {
        m_heartbeatTimer->stop();
    }
    m_removedAt = QDateTime::currentDateTime();
    QFile::remove(m_path);
}

void CrashMarker::removeForStateSave()
{
    if (m_removedAt.isValid()) {
        return;  // already removed by a stronger signal (commitData, WM_ENDSESSION, quit)
    }
    m_removedAt = QDateTime::currentDateTime();
    QFile::remove(m_path);
}

void CrashMarker::removeAt(const QString& path)
{
    if (!path.isEmpty()) {
        QFile::remove(path);
    }
}

QString CrashMarker::readFileTail(const QString& path, qint64 maxBytes)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const qint64 size = file.size();
    if (maxBytes < size) {
        file.seek(size - maxBytes);
    }
    return QString::fromUtf8(file.read(maxBytes));
}

void CrashMarker::heartbeat()
{
    if (m_removedAt.isValid()) {
        // The marker was removed because the OS seemed to be ending the session. If we are still
        // here well afterwards, the session did not end after all (a checkpoint, a cancelled
        // shutdown): bring the marker back so crashes are detected for the rest of the run.
        if (m_removedAt.secsTo(QDateTime::currentDateTime()) < StateSaveReArmSeconds) {
            return;
        }
        touch();
        return;
    }
    writeMarker(QDateTime::currentDateTimeUtc());
}

void CrashMarker::writeMarker(const QDateTime& lastHeartbeat)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("launcherVersion"), BuildConfig.printableVersionString());
    obj.insert(QStringLiteral("startedAt"), m_startedAt.toUTC().toString(Qt::ISODate));
    obj.insert(QStringLiteral("lastHeartbeat"), lastHeartbeat.toString(Qt::ISODate));
    obj.insert(QStringLiteral("bootId"), bootIdentity());

    try {
        FS::write(m_path, QJsonDocument(obj).toJson(QJsonDocument::Compact));
        m_writeFailed = false;
    } catch ([[maybe_unused]] const Exception& e) {
        // e.g. the portable data folder is on a drive that went away: not worth a dialog, and
        // not worth a warning every 30 seconds either.
        if (!m_writeFailed) {
            m_writeFailed = true;
            qWarning() << "Could not write the crash marker" << m_path;
        }
    }
}

QString CrashMarker::bootIdentity()
{
#if defined(Q_OS_WIN)
    // Milliseconds since boot, including sleep/hibernate. now - uptime is the boot time; it only
    // moves when the machine actually reboots (fast startup included). Rounded to the minute so
    // small clock adjustments cannot make one boot look like two.
    const qint64 bootSecs = QDateTime::currentDateTimeUtc().toSecsSinceEpoch() - qint64(GetTickCount64()) / 1000;
    return QStringLiteral("windows-boot:") + QString::number(bootSecs / 60);
#elif defined(Q_OS_LINUX)
    QFile bootId(QStringLiteral("/proc/sys/kernel/random/boot_id"));
    if (bootId.open(QIODevice::ReadOnly)) {
        const QString id = QString::fromLatin1(bootId.readAll().trimmed());
        if (!id.isEmpty()) {
            return QStringLiteral("linux-boot:") + id;
        }
    }
    return {};
#elif defined(Q_OS_MACOS)
    timeval bootTime {};
    size_t size = sizeof(bootTime);
    int mib[2] = { CTL_KERN, KERN_BOOTTIME };
    if (sysctl(mib, 2, &bootTime, &size, nullptr, 0) == 0) {
        return QStringLiteral("macos-boot:") + QString::number(qint64(bootTime.tv_sec));
    }
    return {};
#else
    return {};
#endif
}
