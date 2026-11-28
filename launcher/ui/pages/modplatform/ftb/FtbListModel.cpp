/*
 * Copyright 2020-2021 Jamie Mansfield <jmansfield@cadixdev.org>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "FtbListModel.h"

#include "Application.h"
#include "BuildConfig.h"
#include "Json.h"
#include "ui/widgets/ProjectItem.h"

#include <QLocale>
#include <QPainter>

namespace Ftb {

ListModel::ListModel(QObject* parent) : QAbstractListModel(parent) {}

ListModel::~ListModel() {}

int ListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_modpacks.size();
}

int ListModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : 1;
}

QVariant ListModel::data(const QModelIndex& index, int role) const
{
    int pos = index.row();
    if (pos >= m_modpacks.size() || pos < 0 || !index.isValid()) {
        return QString("INVALID INDEX %1").arg(pos);
    }

    FTB::Modpack pack = m_modpacks.at(pos);
    if (role == Qt::DisplayRole) {
        return pack.name;
    } else if (role == Qt::ToolTipRole) {
        return pack.synopsis;
    } else if (role == Qt::DecorationRole) {
        auto iter = m_logoMap.find(pack.safeName);
        if (iter != m_logoMap.end()) {
            // found: either loaded, or a request is already in flight
            return iter->result;
        }
        // no placeholder icon: while the logo is missing, the delegate draws a coloured
        // initial tile instead of the camera placeholder
        for (auto art : pack.art) {
            if (art.type == "square") {
                ((ListModel*)this)->requestLogo(pack.safeName, art.url);
            }
        }
        return {};
    } else if (role == Qt::UserRole) {
        QVariant v;
        v.setValue(pack);
        return v;
    } else if (role == Qt::SizeHintRole) {
        return QSize(0, 58);
    } else if (role == UserDataTypes::TITLE) {
        return pack.name;
    } else if (role == UserDataTypes::DESCRIPTION) {
        return pack.synopsis;
    } else if (role == UserDataTypes::METADATA) {
        // the fields the model exposes: download counts. Loader and Minecraft version are not
        // part of the pack data, so they are omitted from the row.
        return tr("%1 installs · %2 plays").arg(QLocale().toString(pack.installs), QLocale().toString(pack.plays));
    }

    return QVariant();
}

void ListModel::getLogo(const QString& logo, const QString& logoUrl, LogoCallback callback)
{
    if (m_logoMap.contains(logo)) {
        callback(APPLICATION->metacache()->resolveEntry("FTBPacks", QString("logos/%1").arg(logo))->getFullPath());
    } else {
        requestLogo(logo, logoUrl);
    }
}

void ListModel::request()
{
    m_aborted = false;

    beginResetModel();
    m_modpacks.clear();
    endResetModel();

    auto netJob = makeShared<NetJob>("Ftb::Request", APPLICATION->network());
    auto url = QString(BuildConfig.FTB_API_BASE_URL + "/modpack/all");
    auto [action, response] = Net::Download::makeByteArray(QUrl(url));
    netJob->addNetAction(action);
    m_jobPtr = netJob;
    m_jobPtr->start();

    QObject::connect(netJob.get(), &NetJob::succeeded, this, [this, response] { requestFinished(response); });
    QObject::connect(netJob.get(), &NetJob::failed, this, &ListModel::requestFailed);
}

void ListModel::abortRequest()
{
    m_aborted = m_jobPtr->abort();
    m_jobPtr.reset();
}

void ListModel::requestFinished(QByteArray* responsePtr)
{
    // NOTE(TheKodeToad): moving the response out to avoid it from being destroyed by m_jobPtr.reset()
    QByteArray response = std::move(*responsePtr);
    m_jobPtr.reset();
    m_remainingPacks.clear();

    QJsonParseError parse_error{};
    QJsonDocument doc = QJsonDocument::fromJson(response, &parse_error);
    if (parse_error.error != QJsonParseError::NoError) {
        qWarning() << "Error while parsing JSON response from FTB at " << parse_error.offset << " reason: " << parse_error.errorString();
        qWarning() << response;
        return;
    }

    auto packs = doc.object().value("packs").toArray();
    for (auto pack : packs) {
        auto packId = pack.toInt();
        m_remainingPacks.append(packId);
    }

    if (!m_remainingPacks.isEmpty()) {
        m_currentPack = m_remainingPacks.at(0);
        requestPack();
    }
}

void ListModel::requestFailed(QString)
{
    m_jobPtr.reset();
    m_remainingPacks.clear();
}

void ListModel::requestPack()
{
    auto netJob = makeShared<NetJob>("Ftb::Search", APPLICATION->network());
    auto searchUrl = QString(BuildConfig.FTB_API_BASE_URL + "/modpack/%1").arg(m_currentPack);
    auto [action, response] = Net::Download::makeByteArray(QUrl(searchUrl));
    netJob->addNetAction(action);
    m_jobPtr = netJob;
    m_jobPtr->start();

    QObject::connect(netJob.get(), &NetJob::succeeded, this, [this, response] { packRequestFinished(response); });
    QObject::connect(netJob.get(), &NetJob::failed, this, &ListModel::packRequestFailed);
}

void ListModel::packRequestFinished(QByteArray* responsePtr)
{
    if (!m_jobPtr || m_aborted)
        return;

    // NOTE(TheKodeToad): moving the response out to avoid it from being destroyed by jobPtr.reset()
    QByteArray response = std::move(*responsePtr);

    m_jobPtr.reset();
    m_remainingPacks.removeOne(m_currentPack);

    QJsonParseError parse_error;
    QJsonDocument doc = QJsonDocument::fromJson(response, &parse_error);

    if (parse_error.error != QJsonParseError::NoError) {
        qWarning() << "Error while parsing JSON response from FTB at " << parse_error.offset << " reason: " << parse_error.errorString();
        qWarning() << response;
        return;
    }

    auto obj = doc.object();

    FTB::Modpack pack;
    try {
        FTB::loadModpack(pack, obj);
    } catch (const JSONValidationError& e) {
        qDebug() << QString::fromUtf8(response);
        qWarning() << "Error while reading pack manifest from FTB: " << e.cause();
        return;
    }

    // Since there is no guarantee that packs have a version, this will just
    // ignore those "dud" packs.
    if (pack.versions.empty()) {
        qWarning() << "FTB Pack " << pack.id << " ignored. reason: lacking any versions";
    } else {
        beginInsertRows(QModelIndex(), m_modpacks.size(), m_modpacks.size());
        m_modpacks.append(pack);
        endInsertRows();
    }

    if (!m_remainingPacks.isEmpty()) {
        m_currentPack = m_remainingPacks.at(0);
        requestPack();
    }
}

void ListModel::packRequestFailed(QString)
{
    m_jobPtr.reset();
    m_remainingPacks.removeOne(m_currentPack);
}

void ListModel::logoLoaded(QString logo)
{
    auto& logoObj = m_logoMap[logo];
    logoObj.downloadJob.reset();
    logoObj.result = QIcon(logoObj.fullpath);
    for (int i = 0; i < m_modpacks.size(); i++) {
        if (m_modpacks[i].safeName == logo) {
            emit dataChanged(createIndex(i, 0), createIndex(i, 0), { Qt::DecorationRole });
        }
    }
}

void ListModel::logoFailed(QString logo)
{
    m_logoMap[logo].failed = true;
    m_logoMap[logo].downloadJob.reset();
}

void ListModel::requestLogo(QString logo, QString url)
{
    if (m_logoMap.contains(logo)) {
        return;
    }

    MetaEntryPtr entry = APPLICATION->metacache()->resolveEntry("FTBPacks", QString("logos/%1").arg(logo));

    auto job = makeShared<NetJob>(QString("FTB Icon Download %1").arg(logo), APPLICATION->network());
    job->setAskRetry(false);
    job->addNetAction(Net::Download::makeCached(QUrl(url), entry));

    auto fullPath = entry->getFullPath();
    QObject::connect(job.get(), &NetJob::finished, this, [this, logo, fullPath] { logoLoaded(logo); });

    QObject::connect(job.get(), &NetJob::failed, this, [this, logo] { logoFailed(logo); });

    auto& newLogoEntry = m_logoMap[logo];
    newLogoEntry.downloadJob = job;
    newLogoEntry.fullpath = fullPath;
    job->start();
}

}  // namespace Ftb
