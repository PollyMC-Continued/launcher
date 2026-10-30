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

#include "BugReportTracker.h"

#include <QDateTime>
#include <QDebug>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>
#include <algorithm>

#include "Application.h"
#include "BuildConfig.h"
#include "Exception.h"
#include "FileSystem.h"

namespace {
constexpr int MaxTracked = 30;           // newest reports kept
constexpr int MaxLookupIds = 20;         // the relay answers at most this many IDs per request
constexpr qint64 MaxAgeSeconds = 180LL * 24 * 60 * 60;

// relative on purpose: the launcher's working directory is its data folder
QString filePath()
{
    return QStringLiteral("my_reports.json");
}

// Reports whose current status the user has not been told about yet.
QList<BugReportTracker::Entry> unseenOf(const QList<BugReportTracker::Entry>& list)
{
    QList<BugReportTracker::Entry> unseen;
    for (const auto& e : list)
        if (e.status != e.announced)
            unseen.append(e);
    return unseen;
}
}  // namespace

namespace BugReportTracker {

QList<Entry> load()
{
    QList<Entry> out;
    if (!QFileInfo::exists(filePath()))
        return out;

    QJsonArray array;
    try {
        array = QJsonDocument::fromJson(FS::read(filePath())).array();
    } catch ([[maybe_unused]] const Exception& e) {
        qWarning() << "Could not read the list of sent bug reports";
        return out;
    }

    for (const auto& value : array) {
        const auto o = value.toObject();
        Entry e;
        e.id = o.value("id").toString();
        if (e.id.size() != 10)
            continue;
        e.type = o.value("type").toString();
        e.title = o.value("title").toString();
        e.status = o.value("status").toString(e.status);
        e.version = o.value("version").toString();
        e.announced = o.value("announced").toString(e.announced);
        e.sent = static_cast<qint64>(o.value("sent").toDouble());
        e.updated = static_cast<qint64>(o.value("updated").toDouble());
        out.append(e);
    }
    return out;
}

void save(const QList<Entry>& entries)
{
    QList<Entry> list = entries;
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    list.erase(std::remove_if(list.begin(), list.end(), [now](const Entry& e) { return e.sent > 0 && now - e.sent > MaxAgeSeconds; }),
               list.end());
    std::sort(list.begin(), list.end(), [](const Entry& a, const Entry& b) { return a.sent > b.sent; });
    while (list.size() > MaxTracked)
        list.removeLast();

    QJsonArray array;
    for (const auto& e : list) {
        QJsonObject o;
        o["id"] = e.id;
        o["type"] = e.type;
        o["title"] = e.title;
        o["status"] = e.status;
        o["version"] = e.version;
        o["announced"] = e.announced;
        o["sent"] = static_cast<double>(e.sent);
        o["updated"] = static_cast<double>(e.updated);
        array.append(o);
    }
    try {
        FS::write(filePath(), QJsonDocument(array).toJson(QJsonDocument::Compact));
    } catch ([[maybe_unused]] const Exception& e) {
        qWarning() << "Could not save the list of sent bug reports";
    }
}

void add(const QString& id, const QString& type, const QString& title)
{
    if (id.size() != 10)
        return;
    auto list = load();
    for (const auto& e : list)
        if (e.id == id)
            return;
    Entry e;
    e.id = id;
    e.type = type;
    e.title = title.simplified().left(80);
    e.sent = QDateTime::currentSecsSinceEpoch();
    e.updated = e.sent;
    list.append(e);
    save(list);
}

void markAnnounced(const QStringList& ids)
{
    auto list = load();
    for (auto& e : list)
        if (ids.isEmpty() || ids.contains(e.id))
            e.announced = e.status;
    save(list);
}

bool isFinal(const QString& status)
{
    return status == QLatin1String("fixed") || status == QLatin1String("wont_fix") || status == QLatin1String("duplicate") ||
           status == QLatin1String("not_a_bug");
}

QString displayId(const QString& id)
{
    return id.size() == 10 ? id.left(5) + QLatin1Char('-') + id.mid(5) : id;
}

QString statusText(const QString& status, const QString& version)
{
    if (status == QLatin1String("received"))
        return QObject::tr("Waiting for a dev to check it");
    if (status == QLatin1String("seen"))
        return QObject::tr("Seen - the team is looking into it");
    if (status == QLatin1String("need_info"))
        return QObject::tr("The team needs more information - please reach out on Discord");
    if (status == QLatin1String("fixed_next"))
        return QObject::tr("Fixed - it will be in the next update");
    if (status == QLatin1String("fixed"))
        return version.isEmpty() ? QObject::tr("Fixed") : QObject::tr("Fixed in version %1").arg(version);
    if (status == QLatin1String("wont_fix"))
        return QObject::tr("Won't be fixed");
    if (status == QLatin1String("duplicate"))
        return QObject::tr("Duplicate of another report");
    if (status == QLatin1String("not_a_bug"))
        return QObject::tr("Not a bug");
    return QObject::tr("Updated");  // a status newer than this launcher knows about
}

QString typeText(const QString& type)
{
    if (type == QLatin1String("crash"))
        return QObject::tr("Crash");
    if (type == QLatin1String("feature"))
        return QObject::tr("Suggestion");
    if (type == QLatin1String("other"))
        return QObject::tr("Other");
    return QObject::tr("Bug");
}

}  // namespace BugReportTracker

BugReportStatusChecker::BugReportStatusChecker(QObject* parent) : QObject(parent) {}

void BugReportStatusChecker::check()
{
    if (m_reply)
        return;  // already running; its finished() is still coming

    QStringList ids;
    for (const auto& e : BugReportTracker::load()) {
        if (!BugReportTracker::isFinal(e.status) && ids.size() < MaxLookupIds)
            ids.append(e.id);
    }
    if (ids.isEmpty() || BuildConfig.BUG_REPORT_STATUS_URL.isEmpty()) {
        emit finished(unseenOf(BugReportTracker::load()));
        return;
    }

    QUrl url(BuildConfig.BUG_REPORT_STATUS_URL);
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("ids"), ids.join(QLatin1Char(',')));
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, APPLICATION->getUserAgent().toUtf8());
    request.setTransferTimeout(15000);

    m_reply = APPLICATION->network()->get(request);
    connect(m_reply, &QNetworkReply::finished, this, &BugReportStatusChecker::onReplyFinished);
}

void BugReportStatusChecker::onReplyFinished()
{
    QNetworkReply* reply = m_reply;
    m_reply = nullptr;
    reply->deleteLater();

    auto list = BugReportTracker::load();  // read now, not when the request started: the user may have sent a report meanwhile

    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() == QNetworkReply::NoError && httpStatus == 200) {
        const auto obj = QJsonDocument::fromJson(reply->readAll()).object();
        if (obj.value("ok").toBool()) {
            const auto statuses = obj.value("statuses").toObject();
            QList<BugReportTracker::Entry> kept;
            for (auto e : list) {
                if (!statuses.contains(e.id)) {
                    kept.append(e);  // not asked about (finished) or sent after the request started
                    continue;
                }
                const auto value = statuses.value(e.id);
                if (value.isNull()) {
                    qDebug() << "The relay no longer knows a sent bug report (expired); forgetting it";
                    continue;
                }
                const auto s = value.toObject();
                const QString status = s.value("status").toString();
                if (!status.isEmpty() && (status != e.status || s.value("version").toString() != e.version)) {
                    e.status = status;
                    e.version = s.value("version").toString();
                    e.updated = static_cast<qint64>(s.value("updated").toDouble() / 1000.0);
                }
                kept.append(e);
            }
            list = kept;
            BugReportTracker::save(list);
        }
    } else {
        qDebug() << "Bug report status check failed:" << reply->errorString();  // silent: it will be tried again next start
    }

    emit finished(unseenOf(list));
}
