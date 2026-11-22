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

#include "BedrockArtifactTask.h"

#include <utility>

#include <QCryptographicHash>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>

#include "Application.h"
#include "FileSystem.h"
#include "MMCZip.h"
#include "net/Download.h"

namespace {

/* Finds fileName anywhere below root; the absolute path, or the empty string. */
QString findFileRecursive(const QDir& root, const QString& fileName)
{
    QDirIterator iterator(root.absolutePath(), QStringList() << fileName, QDir::Files, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString candidate = iterator.next();
        if (QFileInfo(candidate).fileName().compare(fileName, Qt::CaseInsensitive) == 0) {
            return candidate;
        }
    }
    return QString();
}

/* The ".verified-<fileName>" marker records the pinned hash that was checked. */
QString markerPath(const QString& destinationDir, const QString& fileName)
{
    return FS::PathCombine(destinationDir, QStringLiteral(".verified-") + fileName);
}

}  // namespace

BedrockArtifactTask::BedrockArtifactTask(Options options) : Task(), m_options(std::move(options)) {}

BedrockArtifactTask::~BedrockArtifactTask() = default;

void BedrockArtifactTask::executeTask()
{
    setStatus(tr("Fetching %1").arg(m_options.artifact.name));

    if (!FS::ensureFolderPathExists(m_options.destinationDir)) {
        emitFailed(tr("Could not create the directory for %1: %2").arg(m_options.artifact.name, m_options.destinationDir));
        return;
    }

    if (isArtifactPresent(m_options.destinationDir, m_options.artifact)) {
        /* Already fetched and verified in an earlier run. */
        emitSucceeded();
        return;
    }

    beginResolve();
}

void BedrockArtifactTask::beginResolve()
{
    if (!m_options.artifact.url.isEmpty()) {
        startDownload(m_options.artifact.url);
        return;
    }

    /*
     * No direct URL: resolve the pinned build through the GitHub releases API
     * of the upstream repository. The listing only locates the download; the
     * pinned SHA-256 decides whether the bytes are used.
     *
     * Every release page is walked, not only the newest one: BedrockOnLinux
     * keeps publishing new engine and winegdk revisions above the pinned
     * builds (the openssl-xcurl release the pin points at is already more
     * than thirty releases below the newest engine), so a pin that fell off
     * the first page would otherwise look unpublished while still being
     * exactly what the pin names.
     */
    const QString apiUrl = QStringLiteral("https://api.github.com/repos/%1/releases?per_page=100&page=%2")
                               .arg(m_options.artifact.repo)
                               .arg(m_releasePage);
    m_job.reset(new NetJob(tr("Resolve %1").arg(m_options.artifact.name), APPLICATION->network()));
    auto [download, data] = Net::Download::makeByteArray(QUrl(apiUrl));
    m_job->addNetAction(download);
    connect(m_job.get(), &Task::succeeded, this, [this, data]() {
        m_releasesData = *data;
        releasesListed();
    });
    connect(m_job.get(), &Task::failed, this, [this](const QString& reason) {
        emitFailed(tr("Could not resolve where %1 is published: %2").arg(m_options.artifact.name, reason));
    });
    m_job->start();
}

void BedrockArtifactTask::releasesListed()
{
    const QJsonDocument document = QJsonDocument::fromJson(m_releasesData);
    if (!document.isArray()) {
        emitFailed(tr("The release listing of %1 did not have the expected shape.").arg(m_options.artifact.repo));
        return;
    }
    const QJsonArray releases = document.array();
    QString resolvedUrl;
    for (const QJsonValue& releaseValue : releases) {
        const QJsonObject release = releaseValue.toObject();
        for (const QJsonValue& assetValue : release.value(QStringLiteral("assets")).toArray()) {
            const QJsonObject asset = assetValue.toObject();
            if (asset.value(QStringLiteral("name")).toString().contains(m_options.artifact.assetPattern)) {
                resolvedUrl = asset.value(QStringLiteral("browser_download_url")).toString();
                break;
            }
        }
        if (!resolvedUrl.isEmpty()) {
            break;
        }
    }
    if (!resolvedUrl.isEmpty()) {
        startDownload(resolvedUrl);
        return;
    }
    /* The pinned asset sits below this page's entries; keep walking. */
    if (releases.size() >= 100) {
        ++m_releasePage;
        beginResolve();
        return;
    }
    emitFailed(tr("No published build of %1 matches the pinned revision (\"%2\"). The pin must be updated, or the binary "
                   "supplied manually, before %3 can be installed.")
                   .arg(m_options.artifact.repo, m_options.artifact.assetPattern, m_options.artifact.name));
}

void BedrockArtifactTask::startDownload(const QString& url)
{
    setStatus(tr("Downloading %1").arg(m_options.artifact.name));

    m_tempPath = FS::PathCombine(m_options.destinationDir, m_options.artifact.fileName + QStringLiteral(".part"));
    m_job.reset(new NetJob(tr("Download %1").arg(m_options.artifact.name), APPLICATION->network()));
    m_job->addNetAction(Net::Download::makeFile(QUrl(url), m_tempPath));
    connect(m_job.get(), &Task::succeeded, this, &BedrockArtifactTask::downloadFinished);
    connect(m_job.get(), &Task::failed, this, [this](const QString& reason) {
        emitFailed(tr("Could not download %1: %2").arg(m_options.artifact.name, reason));
    });
    connect(m_job.get(), &Task::progress, this, &Task::setProgress);
    connect(m_job.get(), &Task::stepProgress, this, &BedrockArtifactTask::propagateStepProgress);
    connect(m_job.get(), &Task::status, this, &Task::setStatus);
    m_job->start();
}

void BedrockArtifactTask::downloadFinished()
{
    if (!m_options.artifact.sha256.isEmpty()) {
        const QString actual = fileSha256(m_tempPath);
        if (actual != m_options.artifact.sha256) {
            QFile::remove(m_tempPath);
            emitFailed(tr("Refused to install %1: the downloaded bytes do not match the pinned SHA-256. The artifact was "
                          "discarded - nothing unverified was installed.")
                           .arg(m_options.artifact.name));
            return;
        }
    }

    m_archivePath = FS::PathCombine(m_options.destinationDir, m_options.artifact.fileName);
    if (QFile::exists(m_archivePath)) {
        QFile::remove(m_archivePath);
    }
    if (!QFile::rename(m_tempPath, m_archivePath)) {
        emitFailed(tr("Could not finalize the download of %1.").arg(m_options.artifact.name));
        return;
    }

    if (!m_options.extract) {
        finishExtraction();
        return;
    }
    extractArchive();
}

void BedrockArtifactTask::extractArchive()
{
    if (m_options.artifact.fileName.endsWith(QStringLiteral(".zip"), Qt::CaseInsensitive)) {
        const auto extracted = MMCZip::extractDir(m_archivePath, m_options.destinationDir);
        if (!extracted.has_value()) {
            emitFailed(tr("Could not unpack %1.").arg(m_options.artifact.name));
            return;
        }
        finishExtraction();
        return;
    }

    /* tar handles the .tar / .tar.gz family the upstream builds ship as. */
    setStatus(tr("Unpacking %1").arg(m_options.artifact.name));
    m_extractProcess = new QProcess(this);
    m_extractProcess->setProgram(QStringLiteral("tar"));
    m_extractProcess->setArguments({ QStringLiteral("-xf"), m_archivePath, QStringLiteral("-C"), m_options.destinationDir });
    connect(m_extractProcess, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus exitStatus) {
        extractionFinished(exitCode, static_cast<int>(exitStatus));
    });
    connect(m_extractProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            emitFailed(tr("Could not start \"tar\" to unpack %1 (is tar installed?).").arg(m_options.artifact.name));
        }
    });
    m_extractProcess->start();
}

void BedrockArtifactTask::extractionFinished(int exitCode, int exitStatus)
{
    Q_UNUSED(exitStatus)
    if (exitCode != 0) {
        emitFailed(tr("Unpacking %1 failed (tar exited with %2).").arg(m_options.artifact.name).arg(exitCode));
        return;
    }
    finishExtraction();
}

void BedrockArtifactTask::finishExtraction()
{
    if (!m_options.expectedInnerFile.isEmpty()) {
        const QString inner = findFileRecursive(QDir(m_options.destinationDir), m_options.expectedInnerFile);
        if (inner.isEmpty()) {
            emitFailed(tr("%1 was fetched, but the expected file \"%2\" was not found in it.")
                           .arg(m_options.artifact.name, m_options.expectedInnerFile));
            return;
        }
        if (m_options.executable) {
            QFile::setPermissions(inner,
                                   QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner | QFileDevice::ReadGroup
                                       | QFileDevice::ExeGroup | QFileDevice::ReadOther | QFileDevice::ExeOther);
        }
    }

    QFile marker(markerPath(m_options.destinationDir, m_options.artifact.fileName));
    if (marker.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        marker.write(m_options.artifact.sha256.toUtf8());
        marker.close();
    }

    emitSucceeded();
}

QString BedrockArtifactTask::fileSha256(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) {
        return QString();
    }
    return QString::fromLatin1(hash.result().toHex());
}

bool BedrockArtifactTask::verifyFileSha256(const QString& path, const QString& expectedHex)
{
    const QString actual = fileSha256(path);
    return !actual.isEmpty() && actual.compare(expectedHex, Qt::CaseInsensitive) == 0;
}

bool BedrockArtifactTask::isArtifactPresent(const QString& destinationDir, const BedrockArtifacts::Artifact& artifact)
{
    const QString filePath = FS::PathCombine(destinationDir, artifact.fileName);
    if (!QFileInfo::exists(filePath)) {
        return false;
    }
    if (artifact.sha256.isEmpty()) {
        return true;
    }
    QFile marker(markerPath(destinationDir, artifact.fileName));
    if (!marker.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QString pinned = QString::fromUtf8(marker.readAll()).trimmed();
    marker.close();
    return pinned == artifact.sha256;
}
