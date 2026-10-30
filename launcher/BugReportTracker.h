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

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

class QNetworkReply;

/// Remembers the bug reports this launcher sent (only the ID, a short title and the last known status) so the
/// team's answer can be shown later. The team sets a status code in Discord; the words shown here are all fixed
/// strings below, which is why they can be translated.
namespace BugReportTracker {

struct Entry {
    QString id;                                           // 10 characters, issued by the report relay
    QString type;                                         // bug / crash / feature / other
    QString title;                                        // first line of what the user wrote
    QString status = QStringLiteral("received");          // last status the server told us
    QString version;                                      // for "fixed": the launcher version with the fix
    QString announced = QStringLiteral("received");       // last status the user was actually told about
    qint64 sent = 0;                                      // epoch seconds
    qint64 updated = 0;                                   // epoch seconds of the last status change
};

QList<Entry> load();
void save(const QList<Entry>& entries);

/// Called right after the relay accepted a report.
void add(const QString& id, const QString& type, const QString& title);

/// The user has now seen these statuses (toast shown or "My Reports" opened). Empty list = all reports.
void markAnnounced(const QStringList& ids = {});

bool isFinal(const QString& status);
QString displayId(const QString& id);       // K7X9F-3QD2M
QString statusText(const QString& status, const QString& version);
QString typeText(const QString& type);
}  // namespace BugReportTracker

/// Asks the relay for the current status of every report that is not finished yet. Never shows errors: a failed
/// check just means "try again next time". finished() is emitted exactly once per check().
class BugReportStatusChecker : public QObject {
    Q_OBJECT

   public:
    explicit BugReportStatusChecker(QObject* parent = nullptr);

    bool isRunning() const { return m_reply != nullptr; }
    void check();

   signals:
    /// Reports whose current status is one the user has not been told about yet (may be empty).
    void finished(const QList<BugReportTracker::Entry>& unseen);

   private:
    void onReplyFinished();

    QNetworkReply* m_reply = nullptr;
};
