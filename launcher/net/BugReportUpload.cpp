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

#include "BugReportUpload.h"

#include <QJsonDocument>
#include <QJsonObject>

BugReportUpload::BugReportUpload(const QUrl& endpoint, const QByteArray& json) : m_json(json)
{
    m_url = endpoint;
    m_sink.reset(new Sink());
}

QNetworkReply* BugReportUpload::getReply(QNetworkRequest& request)
{
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    return m_network->post(request, m_json);
}

auto BugReportUpload::Sink::finalize(QNetworkReply& reply) -> Task::State
{
    if (!finalizeAllValidators(reply)) {
        m_fail_reason = "Failed to finalize validators";
        return Task::State::Failed;
    }

    const int statusCode = reply.attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QJsonParseError jsonError;
    const auto doc = QJsonDocument::fromJson(*output(), &jsonError);
    const auto obj = jsonError.error == QJsonParseError::NoError ? doc.object() : QJsonObject();

    if (reply.error() != QNetworkReply::NoError || statusCode < 200 || statusCode >= 300) {
        // The server sends a short human-readable "error" string; fall back to the network error.
        QString reason = obj.value("error").toString();
        if (reason.isEmpty())
            reason = statusCode == 429 ? QObject::tr("Too many reports, please try again later.") : reply.errorString();
        m_fail_reason = QObject::tr("The report could not be sent: %1").arg(reason);
        return Task::State::Failed;
    }

    if (!obj.value("ok").toBool()) {
        m_fail_reason = QObject::tr("The report server returned an unexpected response.");
        return Task::State::Failed;
    }

    return Task::State::Succeeded;
}
