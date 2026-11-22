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
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *      Copyright 2026 PollyMC-Continued Contributors
 *
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *          http://www.apache.org/licenses/LICENSE-2.0
 *
 *      Unless required by applicable law or agreed to in writing, software
 *      distributed under the License is distributed on an "AS IS" BASIS,
 *      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *      See the License for the specific language governing permissions and
 *      limitations under the License.
 */

#include "LegacyUwpFe3Source.h"

#include <utility>

#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPair>
#include <QRegularExpression>
#include <QTimer>
#include <QXmlStreamReader>

#include "Application.h"

namespace {

/* Endpoints and identifiers, verified live in the Phase 0 report. */
const QString kPrimaryUrl = QStringLiteral("https://fe3.delivery.mp.microsoft.com/ClientWebService/client.asmx");
const QString kSecuredUrl = QStringLiteral("https://fe3.delivery.mp.microsoft.com/ClientWebService/client.asmx/secured");
const QString kActionNamespace = QStringLiteral("http://www.microsoft.com/SoftwareDistribution/Server/ClientWebService");
const QString kUserAgent = QStringLiteral("Windows-Update-Agent/10.0.10011.16384 Client-Protocol/1.81");

const int kSoapTimeoutSeconds = 30;

QString soapEnvelope(const QString& body)
{
    return QStringLiteral(
               "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
               "<soap:Envelope xmlns:soap=\"http://schemas.xmlsoap.org/soap/envelope/\">"
               "<soap:Body>")
           + body
           + QStringLiteral("</soap:Body></soap:Envelope>");
}

/*
 * Envelope bodies. The action sequence, the category ids, the user agent and
 * the returned fields are verified; the shapes follow MS-WUSP. See the
 * defensive note in the header.
 */
QString getConfigBody()
{
    return QStringLiteral("<GetConfig xmlns=\"%1\"/>").arg(kActionNamespace);
}

QString getCookieBody()
{
    return QStringLiteral(
               "<GetCookie xmlns=\"%1\">"
               "<oldCookie></oldCookie>"
               "<lastChange>2015-01-01T00:00:00.0000000Z</lastChange>"
               "<currentTime>2020-01-01T00:00:00.0000000Z</currentTime>"
               "<protocolVersion>1.81</protocolVersion>"
               "</GetCookie>")
        .arg(kActionNamespace);
}

QString syncUpdatesBody(const QString& cookie, const QString& category)
{
    return QStringLiteral(
               "<SyncUpdates xmlns=\"%1\">"
               "%2"
               "<parameters>"
               "<ExpressQuery>false</ExpressQuery>"
               "<InstalledNonLeafUpdateIDs/>"
               "<OtherCachedUpdateIDs/>"
               "<SkipSecuritySync>false</SkipSecuritySync>"
               "<NeedTwoGroupScopes>false</NeedTwoGroupScopes>"
               "<FilterAppCategoryIds>"
               "<CategoryIdentifier><Id>%3</Id></CategoryIdentifier>"
               "</FilterAppCategoryIds>"
               "<AlsoGetPrerequisites>false</AlsoGetPrerequisites>"
               "</parameters>"
               "</SyncUpdates>")
        .arg(kActionNamespace, cookie, category);
}

QString getExtendedUpdateInfo2Body(const QString& cookie, qint64 revisionId)
{
    return QStringLiteral(
               "<GetExtendedUpdateInfo2 xmlns=\"%1\">"
               "%2"
               "<revisionIds>"
               "<int>%3</int>"
               "</revisionIds>"
               "<infoTypes>"
               "<XmlUpdateFragmentType>FileUrl</XmlUpdateFragmentType>"
               "<XmlUpdateFragmentType>Xml</XmlUpdateFragmentType>"
               "</infoTypes>"
               "<locales>"
               "<string>en-US</string>"
               "<string>en</string>"
               "</locales>"
               "</GetExtendedUpdateInfo2>")
        .arg(kActionNamespace, cookie, QString::number(revisionId));
}

/* Extracts the <cookie> element to re-send, out of a GetCookie response. */
bool extractCookie(const QByteArray& response, QString* cookie, QString* error)
{
    QXmlStreamReader reader(response);
    QString expiration;
    QString encryptedData;
    bool inExpiration = false;
    bool inEncryptedData = false;
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement()) {
            const QString name = reader.name().toString();
            if (name.compare(QLatin1String("Expiration"), Qt::CaseInsensitive) == 0) {
                inExpiration = true;
            } else if (name.compare(QLatin1String("EncryptedData"), Qt::CaseInsensitive) == 0) {
                inEncryptedData = true;
            }
        } else if (reader.isCharacters() || reader.isCDATA()) {
            if (inExpiration) {
                expiration += reader.text().toString();
            } else if (inEncryptedData) {
                encryptedData += reader.text().toString();
            }
        } else if (reader.isEndElement()) {
            inExpiration = false;
            inEncryptedData = false;
        }
    }
    if (reader.hasError()) {
        *error = QObject::tr("The cookie response was not valid XML: %1").arg(reader.errorString());
        return false;
    }
    if (encryptedData.isEmpty()) {
        *error = QObject::tr("The cookie response carried no EncryptedData element (the service replied, but not as expected).");
        return false;
    }
    *cookie = QStringLiteral("<cookie><Expiration>%1</Expiration><EncryptedData>%2</EncryptedData></cookie>").arg(expiration, encryptedData);
    return true;
}

/*
 * Collects update blocks out of a SyncUpdates response: the pairs of
 * (update id GUID, revision id) it advertises.
 */
bool extractUpdates(const QByteArray& response, QList<QPair<QString, qint64>>& updates, QString* error)
{
    QXmlStreamReader reader(response);
    QString currentId;
    QString currentRevision;
    bool inId = false;
    bool inRevision = false;
    int inUpdateDepth = 0;
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement()) {
            const QString name = reader.name().toString();
            if (name.compare(QLatin1String("Update"), Qt::CaseInsensitive) == 0) {
                inUpdateDepth++;
                currentId.clear();
                currentRevision.clear();
            } else if (name.compare(QLatin1String("ID"), Qt::CaseInsensitive) == 0 && inUpdateDepth > 0) {
                inId = true;
            } else if (name.compare(QLatin1String("RevisionId"), Qt::CaseInsensitive) == 0 && inUpdateDepth > 0) {
                inRevision = true;
            }
        } else if (reader.isCharacters() || reader.isCDATA()) {
            if (inId) {
                currentId += reader.text().toString();
            } else if (inRevision) {
                currentRevision += reader.text().toString();
            }
        } else if (reader.isEndElement()) {
            const QString name = reader.name().toString();
            if (name.compare(QLatin1String("ID"), Qt::CaseInsensitive) == 0) {
                inId = false;
            } else if (name.compare(QLatin1String("RevisionId"), Qt::CaseInsensitive) == 0) {
                inRevision = false;
            } else if (name.compare(QLatin1String("Update"), Qt::CaseInsensitive) == 0 && inUpdateDepth > 0) {
                inUpdateDepth--;
                if (!currentId.isEmpty() && !currentRevision.isEmpty()) {
                    updates.append(qMakePair(currentId, currentRevision.toLongLong()));
                }
                currentId.clear();
                currentRevision.clear();
            }
        }
    }
    if (reader.hasError()) {
        *error = QObject::tr("The update listing was not valid XML: %1").arg(reader.errorString());
        return false;
    }
    return true;
}

/* Collects every <FileLocation> (Url + FileDigest) of the response. */
bool extractFileLocations(const QByteArray& response, QList<LegacyUwpFe3Source::FileLocation>& files, QString* error)
{
    QXmlStreamReader reader(response);
    QString currentUrl;
    QString currentDigest;
    bool inUrl = false;
    bool inDigest = false;
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement()) {
            const QString name = reader.name().toString();
            if (name.compare(QLatin1String("Url"), Qt::CaseInsensitive) == 0) {
                inUrl = true;
                currentUrl.clear();
            } else if (name.compare(QLatin1String("FileDigest"), Qt::CaseInsensitive) == 0) {
                inDigest = true;
                currentDigest.clear();
            }
        } else if ((reader.isCharacters() || reader.isCDATA()) && (inUrl || inDigest)) {
            if (inUrl) {
                currentUrl += reader.text().toString();
            } else {
                currentDigest += reader.text().toString();
            }
        } else if (reader.isEndElement()) {
            const QString name = reader.name().toString();
            if (name.compare(QLatin1String("Url"), Qt::CaseInsensitive) == 0) {
                inUrl = false;
            } else if (name.compare(QLatin1String("FileDigest"), Qt::CaseInsensitive) == 0) {
                inDigest = false;
            } else if (name.compare(QLatin1String("FileLocation"), Qt::CaseInsensitive) == 0) {
                if (!currentUrl.isEmpty()) {
                    LegacyUwpFe3Source::FileLocation location;
                    location.url = currentUrl;
                    location.sha1Base64 = currentDigest;
                    files.append(location);
                }
                currentUrl.clear();
                currentDigest.clear();
            }
        }
    }
    if (reader.hasError()) {
        *error = QObject::tr("The file location response was not valid XML: %1").arg(reader.errorString());
        return false;
    }
    return true;
}

}  // namespace

bool LegacyUwpFe3Source::soapCall(const QString& endpoint, const QString& action, const QString& body, QByteArray* response, QString* error)
{
    QNetworkRequest request{QUrl(endpoint)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("text/xml; charset=utf-8"));
    request.setRawHeader("SOAPAction", QStringLiteral("\"%1\"").arg(action).toUtf8());
    request.setRawHeader("User-Agent", kUserAgent.toUtf8());

    QNetworkAccessManager* network = APPLICATION->network();
    QNetworkReply* reply = network->post(request, body.toUtf8());

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer::singleShot(kSoapTimeoutSeconds * 1000, &loop, &QEventLoop::quit);
    loop.exec();

    if (!reply->isFinished()) {
        reply->abort();
        reply->deleteLater();
        *error = QObject::tr("The delivery service (%1) did not answer within %2 seconds.").arg(endpoint).arg(kSoapTimeoutSeconds);
        return false;
    }
    const QNetworkReply::NetworkError networkError = reply->error();
    if (networkError != QNetworkReply::NoError) {
        *error = QObject::tr("The delivery service (%1) refused the request: %2").arg(endpoint, reply->errorString());
        reply->deleteLater();
        return false;
    }
    *response = reply->readAll();
    reply->deleteLater();

    if (response->isEmpty()) {
        *error = QObject::tr("The delivery service (%1) returned an empty response.").arg(endpoint);
        return false;
    }
    /* A SOAP Fault is a failure with its message, not a success. */
    const QRegularExpression faultPattern(QStringLiteral("<\\s*[fF]ault\\s*>"));
    if (faultPattern.match(QString::fromUtf8(*response)).hasMatch()) {
        QRegularExpression messagePattern(
            QStringLiteral("<\\s*[fF]aultstring\\s*>\\s*<!\\[CDATA\\[([^\\]]*)\\]\\]>|<\\s*[fF]aultstring\\s*>([^<]*)"));
        const QRegularExpressionMatch match = messagePattern.match(QString::fromUtf8(*response));
        const QString faultMessage = match.captured(0).isEmpty() ? QString() : (match.captured(1).isEmpty() ? match.captured(2) : match.captured(1));
        *error = QObject::tr("The delivery service answered with a SOAP fault (%1): %2")
                     .arg(action, faultMessage.isEmpty() ? QObject::tr("no message") : faultMessage);
        return false;
    }
    return true;
}

QString LegacyUwpFe3Source::appCategory(BedrockVersion::Channel channel)
{
    if (channel == BedrockVersion::Channel::Preview) {
        return QStringLiteral("188f32fc-5eaa-45a8-9f78-7dde4322d131");
    }
    return QStringLiteral("d25480ca-36aa-46e6-b76b-39608d49558c");
}

bool LegacyUwpFe3Source::getConfig(QString* error)
{
    QByteArray response;
    if (!soapCall(kPrimaryUrl, kActionNamespace + QStringLiteral("/GetConfig"), soapEnvelope(getConfigBody()), &response, error)) {
        return false;
    }
    return true;
}

bool LegacyUwpFe3Source::getCookie(QString* cookie, QString* error)
{
    QByteArray response;
    if (!soapCall(kPrimaryUrl, kActionNamespace + QStringLiteral("/GetCookie"), soapEnvelope(getCookieBody()), &response, error)) {
        return false;
    }
    return extractCookie(response, cookie, error);
}

bool LegacyUwpFe3Source::syncUpdates(const QString& cookie,
                                     BedrockVersion::Channel channel,
                                     QList<QPair<QString, qint64>>& updates,
                                     QString* error)
{
    QByteArray response;
    const QString action = kActionNamespace + QStringLiteral("/SyncUpdates");
    if (!soapCall(kPrimaryUrl, action, soapEnvelope(syncUpdatesBody(cookie, appCategory(channel))), &response, error)) {
        return false;
    }
    return extractUpdates(response, updates, error);
}

bool LegacyUwpFe3Source::getExtendedUpdateInfo2(const QString& cookie, qint64 revisionId, QList<FileLocation>& files, QString* error)
{
    QByteArray response;
    const QString action = kActionNamespace + QStringLiteral("/GetExtendedUpdateInfo2");
    if (!soapCall(kSecuredUrl, action, soapEnvelope(getExtendedUpdateInfo2Body(cookie, revisionId)), &response, error)) {
        return false;
    }
    return extractFileLocations(response, files, error);
}

QList<LegacyUwpFe3Source::FileLocation> LegacyUwpFe3Source::filesForUpdates(const QStringList& updateIds,
                                                                             BedrockVersion::Channel channel,
                                                                             QString* error)
{
    /*
     * The verified action sequence, resolved per update id: GetConfig,
     * GetCookie, SyncUpdates for the channel's app category, then
     * GetExtendedUpdateInfo2 for the matching revision. Release channel
     * downloads need no user ticket - the service itself is the gate.
     */
    if (getConfig(error)) {
        QString cookie;
        if (!getCookie(&cookie, error)) {
            return {};
        }
        QList<QPair<QString, qint64>> updates;
        if (!syncUpdates(cookie, channel, updates, error)) {
            return {};
        }
        for (const QString& updateId : updateIds) {
            for (const QPair<QString, qint64>& candidate : updates) {
                if (candidate.first.compare(updateId, Qt::CaseInsensitive) == 0 && candidate.second > 0) {
                    QList<FileLocation> files;
                    if (getExtendedUpdateInfo2(cookie, candidate.second, files, error) && !files.isEmpty()) {
                        return files;
                    }
                    /* Try the next matching update id before giving up. */
                    break;
                }
            }
        }
        *error = QObject::tr(
            "None of the known update ids (%1) is currently published by the delivery service. The service's history depth is "
            "limited; very old versions may no longer be listed.")
                     .arg(updateIds.join(QStringLiteral(", ")));
        return {};
    }
    return {};
}
