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

#include "BedrockVersionList.h"

#include <algorithm>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMap>
#include <QSet>
#include <QUrl>

#include "Application.h"
#include "Exception.h"
#include "FileSystem.h"
#include "net/Download.h"
#include "net/HttpMetaCache.h"
#include "net/NetJob.h"

/*
 * Translation helper for the load task (a plain Task subclass, so no own tr()
 * context): user-visible strings it builds carry the list's context.
 */
static inline QString bedrockListTr(const char* text)
{
    return QCoreApplication::translate("BedrockVersionList", text);
}

namespace {

/* Version databases, verified live in the Phase 0 report (2026-11-21) and
 * re-verified (schemas, coverage, live fetch) while implementing this step.
 * GdkLinks and Metadata are the load-bearing, MIT-licensed sources and are
 * primary; the Rayth Network list is used as a mirror-only supplement. */
struct BedrockSourceInfo {
    const char* cacheName;
    const char* url;
};

const QList<BedrockSourceInfo> kBedrockSources = {
    { "urls.json", "https://raw.githubusercontent.com/MinecraftBedrockArchiver/GdkLinks/master/urls.json" },
    { "w10_meta.json", "https://raw.githubusercontent.com/MinecraftBedrockArchiver/Metadata/master/w10_meta.json" },
    { "w10_preview_meta.json", "https://raw.githubusercontent.com/MinecraftBedrockArchiver/Metadata/master/w10_preview_meta.json" },
    { "rayth_versions.json", "https://www.raythnetwork.co.uk/versions.php?type=json" },
};

/* The shipped emergency snapshot (resources/bedrock), last resort when neither
 * the network nor a cached copy is available. */
const QString kSnapshotResourcePath = QStringLiteral(":/bedrock/bedrock_version_snapshot.json");

}  // namespace

/*
 * Loads and merges the version databases. Deliberately not a QObject with its
 * own signals: the standard Task signals (succeeded/failed/progress/status)
 * are enough, and the list's refreshSucceeded / refreshFailed are emitted as a
 * friend at the end.
 */
class BedrockVersionListLoadTask : public Task {
   public:
    BedrockVersionListLoadTask(BedrockVersionList* list, bool forceReload) : Task(), m_list(list), m_forceReload(forceReload) {}

    ~BedrockVersionListLoadTask() override = default;

    void executeTask() override;

   private:
    void downloadFinished();

    /* Reads a downloaded (or previously cached) source database; empty when unavailable. */
    QByteArray readSourceData(const QString& cacheName);

    void parseMetadata(const QByteArray& data, BedrockVersion::Channel channel, QMap<QString, BedrockVersion::Ptr>& merged, QStringList& sourcesUsed);
    void parseGdkLinks(const QByteArray& data, QMap<QString, BedrockVersion::Ptr>& merged, QStringList& sourcesUsed);
    void parseRaythUwpList(const QByteArray& data, QMap<QString, BedrockVersion::Ptr>& merged, QStringList& sourcesUsed);
    void parseShippedSnapshot(QMap<QString, BedrockVersion::Ptr>& merged);

    static BedrockVersion::Ptr ensureVersion(QMap<QString, BedrockVersion::Ptr>& merged, const QString& versionId, bool& newlyCreated);
    static void classifyFromExtension(BedrockVersion* version, const QString& fileNameOrUrl);

    BedrockVersionList* m_list = nullptr;
    bool m_forceReload = false;
    NetJob::Ptr m_netJob;
    QHash<QString, MetaEntryPtr> m_entries;
    QSet<QString> m_freshSources;
    QString m_netFailureReason;
};

void BedrockVersionListLoadTask::executeTask()
{
    if (m_list->m_loaded && !m_forceReload) {
        emitSucceeded();
        return;
    }

    setStatus(bedrockListTr("Downloading Bedrock version data"));
    m_netJob.reset(new NetJob(bedrockListTr("Download Bedrock version data"), APPLICATION->network()));
    for (const BedrockSourceInfo& source : kBedrockSources) {
        auto entry = APPLICATION->metacache()->resolveEntry("bedrock", source.cacheName);
        if (m_forceReload) {
            // manual refresh: drop revalidation headers so servers cannot answer 304
            entry->setETag({});
            entry->setRemoteChangedTimestamp({});
        }
        entry->setStale(true);
        m_entries.insert(QString::fromLatin1(source.cacheName), entry);
        auto download = Net::Download::makeCached(QUrl(QString::fromLatin1(source.url)), entry);
        connect(download.get(), &Task::succeeded, this,
                [this, cacheName = QString::fromLatin1(source.cacheName)]() { m_freshSources.insert(cacheName); });
        m_netJob->addNetAction(download);
    }
    m_netJob->setAskRetry(false);
    connect(m_netJob.get(), &Task::succeeded, this, &BedrockVersionListLoadTask::downloadFinished);
    connect(m_netJob.get(), &Task::failed, this,
            [this](const QString& reason) {
                // A failed refresh keeps whatever was loaded before (cached
                // copies survive failed downloads, and the shipped snapshot is
                // the last resort) instead of emptying the list - the behavior
                // BedrockLauncher proved with its "keeping the local copy".
                m_netFailureReason = reason;
                downloadFinished();
            });
    connect(m_netJob.get(), &Task::progress, this, &Task::setProgress);
    connect(m_netJob.get(), &Task::stepProgress, this, &BedrockVersionListLoadTask::propagateStepProgress);
    connect(m_netJob.get(), &Task::status, this, &Task::setStatus);
    connect(m_netJob.get(), &Task::details, this, &Task::setDetails);
    m_netJob->start();
}

void BedrockVersionListLoadTask::downloadFinished()
{
    setStatus(bedrockListTr("Merging Bedrock version databases"));

    QMap<QString, BedrockVersion::Ptr> merged;
    QStringList sourcesUsed;

    /* The Metadata repositories are the channel authority (and provide history,
     * file names, SHA-256 hashes and update ids); GdkLinks is primary for
     * package URLs; the Rayth Network list is a mirror-only supplement. */
    parseMetadata(readSourceData("w10_meta.json"), BedrockVersion::Channel::Release, merged, sourcesUsed);
    parseMetadata(readSourceData("w10_preview_meta.json"), BedrockVersion::Channel::Preview, merged, sourcesUsed);
    parseGdkLinks(readSourceData("urls.json"), merged, sourcesUsed);
    parseRaythUwpList(readSourceData("rayth_versions.json"), merged, sourcesUsed);

    bool fromSnapshot = false;
    if (merged.isEmpty()) {
        fromSnapshot = true;
        parseShippedSnapshot(merged);
        if (!merged.isEmpty()) {
            sourcesUsed << bedrockListTr("shipped emergency snapshot");
        }
    }

    if (merged.isEmpty()) {
        const QString reason = bedrockListTr("No Bedrock version data could be loaded. Network: %1")
                                   .arg(m_netFailureReason.isEmpty() ? bedrockListTr("no source reachable") : m_netFailureReason);
        m_list->m_statusDescription = reason;
        emit m_list->refreshFailed(reason);
        emitFailed(reason);
        return;
    }

    QList<BedrockVersion::Ptr> versions = merged.values();
    std::sort(versions.begin(), versions.end(),
              [](const BedrockVersion::Ptr& a, const BedrockVersion::Ptr& b) { return BedrockVersion::isNewerThan(a->versionId(), b->versionId()); });

    m_list->m_fromSnapshot = fromSnapshot;
    m_list->m_lastRefreshed = QDateTime::currentDateTime();
    m_list->m_statusDescription = bedrockListTr("%1 versions loaded from %2 (%3)")
                                      .arg(QString::number(versions.count()), sourcesUsed.join(bedrockListTr(", ")),
                                           QLocale::system().toString(m_list->m_lastRefreshed, QLocale::ShortFormat));
    m_list->m_loaded = true;

    QList<BaseVersion::Ptr> baseVersions;
    for (const BedrockVersion::Ptr& version : versions) {
        baseVersions.append(version);
    }
    m_list->updateListData(baseVersions);

    if (!m_netFailureReason.isEmpty()) {
        qWarning() << "Bedrock version list: some sources failed to refresh (" << m_netFailureReason << "); keeping the rest.";
    }

    emit m_list->refreshSucceeded();
    emitSucceeded();
}

QByteArray BedrockVersionListLoadTask::readSourceData(const QString& cacheName)
{
    const MetaEntryPtr entry = m_entries.value(cacheName);
    if (!entry) {
        return {};
    }
    const QString path = entry->getFullPath();
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        return {};
    }
    try {
        /* A source whose refresh failed keeps its previously cached copy on
         * disk (failed downloads never clobber the old file), so this read is
         * the cache fallback. */
        return FS::read(path);
    } catch (const Exception& exception) {
        qWarning() << "Bedrock version list: could not read cached source" << cacheName << ":" << exception.cause();
        return {};
    }
}

BedrockVersion::Ptr BedrockVersionListLoadTask::ensureVersion(QMap<QString, BedrockVersion::Ptr>& merged, const QString& versionId, bool& newlyCreated)
{
    newlyCreated = false;
    if (versionId.isEmpty()) {
        return nullptr;
    }
    const BedrockVersion::Ptr existing = merged.value(versionId);
    if (existing) {
        return existing;
    }
    newlyCreated = true;
    auto created = std::make_shared<BedrockVersion>(versionId);
    merged.insert(versionId, created);
    return created;
}

void BedrockVersionListLoadTask::classifyFromExtension(BedrockVersion* version, const QString& fileNameOrUrl)
{
    if (fileNameOrUrl.endsWith(".msixvc", Qt::CaseInsensitive)) {
        version->setPackaging(BedrockVersion::Packaging::Msixvc);
    } else if (fileNameOrUrl.endsWith(".appx", Qt::CaseInsensitive)) {
        version->setPackaging(BedrockVersion::Packaging::Appx);
    }
}

void BedrockVersionListLoadTask::parseMetadata(const QByteArray& data,
                                               BedrockVersion::Channel channel,
                                               QMap<QString, BedrockVersion::Ptr>& merged,
                                               QStringList& sourcesUsed)
{
    if (data.isEmpty()) {
        return;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        qWarning() << "Bedrock version list: could not parse Metadata source:" << parseError.errorString();
        return;
    }
    const QJsonObject root = document.object();
    bool addedAny = false;
    for (auto it = root.constBegin(); it != root.constEnd(); ++it) {
        const QString versionId = it.key();
        if (versionId.isEmpty() || versionId.startsWith('_')) {
            continue;
        }
        bool newlyCreated = false;
        const BedrockVersion::Ptr version = ensureVersion(merged, versionId, newlyCreated);
        if (!version) {
            continue;
        }
        addedAny = true;
        version->setChannel(channel);

        const QJsonObject versionObject = it.value().toObject();
        const QJsonObject architectures = versionObject.value(QStringLiteral("Archs")).toObject();
        /* The launcher installs the x64 package; other architectures (arm) are ignored. */
        QJsonObject package = architectures.value(QStringLiteral("x64")).toObject();
        if (package.isEmpty() && !architectures.isEmpty()) {
            package = architectures.constBegin().value().toObject();
        }
        if (!package.isEmpty()) {
            const QString fileName = package.value(QStringLiteral("FileName")).toString();
            if (!fileName.isEmpty()) {
                version->setFileName(fileName);
            }
            version->setSha256(package.value(QStringLiteral("Hashes")).toObject().value(QStringLiteral("SHA256")).toString());
            const QJsonArray updateIds = package.value(QStringLiteral("UpdateIds")).toArray();
            for (const QJsonValue& updateId : updateIds) {
                version->addUpdateId(updateId.toString());
            }
        }
    }
    if (addedAny) {
        sourcesUsed << (channel == BedrockVersion::Channel::Release ? bedrockListTr("Metadata releases") : bedrockListTr("Metadata previews"));
    }
}

void BedrockVersionListLoadTask::parseGdkLinks(const QByteArray& data, QMap<QString, BedrockVersion::Ptr>& merged, QStringList& sourcesUsed)
{
    if (data.isEmpty()) {
        return;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        qWarning() << "Bedrock version list: could not parse GdkLinks source:" << parseError.errorString();
        return;
    }
    const QJsonObject root = document.object();
    bool addedAny = false;
    const QStringList channelKeys = { QStringLiteral("release"), QStringLiteral("preview") };
    for (const QString& channelKey : channelKeys) {
        const BedrockVersion::Channel channel = channelKey == QLatin1String("release") ? BedrockVersion::Channel::Release : BedrockVersion::Channel::Preview;
        const QJsonObject channelObject = root.value(channelKey).toObject();
        for (auto it = channelObject.constBegin(); it != channelObject.constEnd(); ++it) {
            const QString versionId = it.key();
            bool newlyCreated = false;
            const BedrockVersion::Ptr version = ensureVersion(merged, versionId, newlyCreated);
            if (!version) {
                continue;
            }
            addedAny = true;
            if (newlyCreated) {
                version->setChannel(channel);
            } else if (version->channel() != channel) {
                /* Metadata stays the channel authority; note the disagreement. */
                qWarning() << "Bedrock version list: channel disagreement for version" << versionId
                           << "(Metadata:" << (version->channel() == BedrockVersion::Channel::Release ? "release" : "preview") << "GdkLinks:" << channelKey << ")";
            }

            QStringList urls;
            const QJsonArray urlArray = it.value().toArray();
            for (const QJsonValue& value : urlArray) {
                const QString url = value.toString();
                if (url.startsWith(QLatin1String("http"))) {
                    urls.append(url);
                }
            }
            version->setPackageUrls(urls);
            if (!urls.isEmpty()) {
                classifyFromExtension(version.get(), urls.constFirst());
            }
        }
    }
    if (addedAny) {
        sourcesUsed << bedrockListTr("GdkLinks");
    }
}

void BedrockVersionListLoadTask::parseRaythUwpList(const QByteArray& data, QMap<QString, BedrockVersion::Ptr>& merged, QStringList& sourcesUsed)
{
    if (data.isEmpty()) {
        return;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
        qWarning() << "Bedrock version list: could not parse the Rayth Network list:" << parseError.errorString();
        return;
    }
    const QJsonArray root = document.array();
    bool addedAny = false;
    /* Schema: [ [version, updateId GUID, type, arch], ... ] with type
     * 0 = release, 1 = beta, 2 = preview. The dead Beta channel is skipped. */
    for (const QJsonValue& element : root) {
        const QJsonArray row = element.toArray();
        if (row.count() < 2) {
            continue;
        }
        const QString versionId = row.at(0).toString();
        if (versionId.isEmpty()) {
            continue;
        }
        const int type = row.at(2).toInt();
        if (type == 1) {
            continue;
        }
        bool newlyCreated = false;
        const BedrockVersion::Ptr version = ensureVersion(merged, versionId, newlyCreated);
        if (!version) {
            continue;
        }
        addedAny = true;
        if (newlyCreated) {
            /* Mirror only: the channel for versions no other source knows. */
            version->setChannel(type == 2 ? BedrockVersion::Channel::Preview : BedrockVersion::Channel::Release);
        }
        version->addUpdateId(row.at(1).toString());
    }
    if (addedAny) {
        sourcesUsed << bedrockListTr("Rayth Network list (mirror)");
    }
}

void BedrockVersionListLoadTask::parseShippedSnapshot(QMap<QString, BedrockVersion::Ptr>& merged)
{
    QFile snapshotFile(kSnapshotResourcePath);
    if (!snapshotFile.open(QIODevice::ReadOnly)) {
        qWarning() << "Bedrock version list: no shipped snapshot available";
        return;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(snapshotFile.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        qWarning() << "Bedrock version list: could not parse the shipped snapshot:" << parseError.errorString();
        return;
    }
    const QJsonObject root = document.object();
    auto loadChannel = [&merged](const QJsonObject& channelObject, BedrockVersion::Channel channel) {
        for (auto it = channelObject.constBegin(); it != channelObject.constEnd(); ++it) {
            const QString versionId = it.key();
            bool newlyCreated = false;
            const BedrockVersion::Ptr version = ensureVersion(merged, versionId, newlyCreated);
            if (!version) {
                continue;
            }
            version->setChannel(channel);
            const QJsonObject entry = it.value().toObject();
            const QString fileName = entry.value(QStringLiteral("fileName")).toString();
            if (!fileName.isEmpty()) {
                version->setFileName(fileName);
            }
            version->setSha256(entry.value(QStringLiteral("sha256")).toString());
            QStringList urls;
            const QJsonArray urlArray = entry.value(QStringLiteral("urls")).toArray();
            for (const QJsonValue& value : urlArray) {
                const QString url = value.toString();
                if (url.startsWith(QLatin1String("http"))) {
                    urls.append(url);
                }
            }
            version->setPackageUrls(urls);
        }
    };
    loadChannel(root.value(QStringLiteral("release")).toObject(), BedrockVersion::Channel::Release);
    loadChannel(root.value(QStringLiteral("preview")).toObject(), BedrockVersion::Channel::Preview);
}

////// BedrockVersionList //////

BedrockVersionList::BedrockVersionList(QObject* parent) : BaseVersionList(parent) {}

BedrockVersionList* BedrockVersionList::get()
{
    /* Parented to the application so the model's lifetime follows the
     * application; pages keep using this single shared instance. */
    static BedrockVersionList* globalList = new BedrockVersionList(APPLICATION);
    return globalList;
}

Task::Ptr BedrockVersionList::getLoadTask(bool forceReload)
{
    if (m_currentLoadTask && m_currentLoadTask->isRunning()) {
        return m_currentLoadTask;
    }
    m_currentLoadTask.reset(new BedrockVersionListLoadTask(this, forceReload));
    return m_currentLoadTask;
}

Task::Ptr BedrockVersionList::refresh(bool force)
{
    const Task::Ptr task = getLoadTask(force);
    if (!task->isRunning()) {
        task->start();
    }
    return task;
}

bool BedrockVersionList::isLoaded()
{
    return m_loaded;
}

const BaseVersion::Ptr BedrockVersionList::at(int i) const
{
    if (i < 0 || i >= m_versions.count()) {
        return nullptr;
    }
    return m_versions.at(i);
}

int BedrockVersionList::count() const
{
    return m_versions.count();
}

void BedrockVersionList::sortVersions()
{
    /* Newest first. */
    std::stable_sort(m_versions.begin(), m_versions.end(),
                     [](const BedrockVersion::Ptr& a, const BedrockVersion::Ptr& b) { return BedrockVersion::isNewerThan(a->versionId(), b->versionId()); });
}

BaseVersion::Ptr BedrockVersionList::getRecommended() const
{
    for (const BedrockVersion::Ptr& version : m_versions) {
        if (version->channel() == BedrockVersion::Channel::Release) {
            return version;
        }
    }
    return count() > 0 ? at(0) : nullptr;
}

BedrockVersion::Ptr BedrockVersionList::findBedrockVersion(const QString& versionId) const
{
    for (const BedrockVersion::Ptr& version : m_versions) {
        if (version->versionId() == versionId) {
            return version;
        }
    }
    return nullptr;
}

QString BedrockVersionList::statusDescription() const
{
    return m_statusDescription;
}

QDateTime BedrockVersionList::lastRefreshed() const
{
    return m_lastRefreshed;
}

QVariant BedrockVersionList::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= count()) {
        return QVariant();
    }
    const BedrockVersion::Ptr version = m_versions.at(index.row());
    switch (role) {
        case Qt::DisplayRole:
            return QString("%1 \u2014 %2").arg(version->versionId(), version->typeString());
        case Qt::ToolTipRole:
            return tooltipForVersion(version);
        default:
            break;
    }
    return BaseVersionList::data(index, role);
}

void BedrockVersionList::updateListData(QList<BaseVersion::Ptr> versions)
{
    beginResetModel();
    m_versions.clear();
    for (const BaseVersion::Ptr& version : versions) {
        auto bedrockVersion = std::dynamic_pointer_cast<BedrockVersion>(version);
        if (bedrockVersion) {
            m_versions.append(bedrockVersion);
        }
    }
    sortVersions();
    endResetModel();
}

QString BedrockVersionList::tooltipForVersion(const BedrockVersion::Ptr& version) const
{
    QStringList lines;
    lines << tr("Channel: %1").arg(BedrockVersion::channelToString(version->channel()));
    if (version->isLegacyUwp()) {
        lines << tr("Packaging: legacy UWP package (.Appx; extract-and-register pipeline)");
    } else if (version->packaging() == BedrockVersion::Packaging::Msixvc) {
        lines << tr("Packaging: current GDK package (.msixvc; installed by Windows)");
    } else {
        lines << tr("Packaging: unknown");
    }
    if (!version->fileName().isEmpty()) {
        lines << tr("Package file: %1").arg(version->fileName());
    }
    lines << tr("SHA-256 hash: %1").arg(version->hasSha256() ? version->sha256() : tr("unknown"));
    if (version->hasPackageUrls()) {
        lines << tr("Download URLs: %1").arg(QString::number(version->packageUrls().count()));
    } else {
        lines << tr("Download URLs: none yet (resolved by the install pipeline)");
    }
    if (!version->updateIds().isEmpty()) {
        lines << tr("Update IDs: %1").arg(version->updateIds().join(QStringLiteral(", ")));
    }
    return lines.join('\n');
}
