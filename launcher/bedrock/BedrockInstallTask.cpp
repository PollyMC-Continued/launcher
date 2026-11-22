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

#include "BedrockInstallTask.h"

#include <utility>

#include <QCryptographicHash>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QMessageBox>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QTextStream>

#include "Application.h"
#include "BedrockArtifactTask.h"
#include "BedrockAsync.h"
#include "BedrockInstance.h"
#include "BedrockVersionList.h"
#include "FileSystem.h"
#include "GdkPackageInstaller.h"
#include "LegacyUwpFe3Source.h"
#include "MMCZip.h"
#include "XodusLicensingClient.h"
#include "net/ChecksumValidator.h"
#include "net/Download.h"

namespace {
/* The install state marker inside the instance's game folder. */
const QString kMarkerFileName = QStringLiteral(".bedrock_install");
}  // namespace

namespace BedrockPaths {

QString bedrockDataDir()
{
    return FS::PathCombine(APPLICATION->dataRoot(), QStringLiteral("bedrock"));
}

QString packagesDir()
{
    return FS::PathCombine(bedrockDataDir(), QStringLiteral("packages"));
}

QString packageDir(const QString& versionId)
{
    return FS::PathCombine(packagesDir(), versionId);
}

QString enginesDir()
{
    return FS::PathCombine(bedrockDataDir(), QStringLiteral("engines"));
}

QString umuDir()
{
    return FS::PathCombine(bedrockDataDir(), QStringLiteral("umu"));
}

QString gdkDepsDir()
{
    return FS::PathCombine(bedrockDataDir(), QStringLiteral("gdkdeps"));
}

QString xodusDir()
{
    return FS::PathCombine(bedrockDataDir(), QStringLiteral("xodus"));
}

QString xodusHomeDir()
{
    return FS::PathCombine(bedrockDataDir(), QStringLiteral("xodus-home"));
}

QString opensslXcurlDir()
{
    return FS::PathCombine(bedrockDataDir(), QStringLiteral("openssl-xcurl"));
}

QString windowsHelperDir()
{
    return FS::PathCombine(bedrockDataDir(), QStringLiteral("helper"));
}

QString instanceGameDir(BedrockInstance* instance)
{
    return instance ? FS::PathCombine(instance->instanceRoot(), QStringLiteral("game")) : QString();
}

QString instanceGameDataDir(BedrockInstance* instance)
{
    return instance ? FS::PathCombine(instance->instanceRoot(), QStringLiteral("gamedata")) : QString();
}

QString instancePrefixDir(BedrockInstance* instance)
{
    return instance ? FS::PathCombine(instance->instanceRoot(), QStringLiteral("proton-prefix")) : QString();
}

}  // namespace BedrockPaths

////// BedrockInstallTask //////

Task::Ptr BedrockInstallTask::create(BedrockInstance* instance)
{
    BedrockVersion::Ptr version;
    if (instance && !instance->bedrockVersion().isEmpty()) {
        version = BedrockVersionList::get()->findBedrockVersion(instance->bedrockVersion());
    }
    return makeShared<BedrockInstallTask>(instance ? instance->instanceRoot() : QString(), version);
}

BedrockInstallTask::BedrockInstallTask(const QString& destinationDir, BedrockVersion::Ptr version)
    : Task(), m_version(std::move(version)), m_destinationDir(destinationDir)
{}

BedrockInstallTask::~BedrockInstallTask() = default;

QString BedrockInstallTask::packageCachePath(const BedrockVersion::Ptr& version)
{
    QString fileName = version->fileName();
    if (fileName.isEmpty()) {
        fileName = QStringLiteral("package.bin");
    }
    return FS::PathCombine(BedrockPaths::packageDir(version->versionId()), fileName);
}

QString BedrockInstallTask::expectedWindowsPackageVersion(const BedrockVersion::Ptr& version)
{
    /* The package file name carries the Windows package version directly. */
    if (!version->fileName().isEmpty()) {
        static const QRegularExpression pattern(QStringLiteral("_([0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+)"));
        const QRegularExpressionMatch match = pattern.match(version->fileName());
        if (match.hasMatch()) {
            return match.captured(1);
        }
    }
    /* "1.26.65.1" -> "1.26.6501.0": the third and fourth components merge. */
    const QStringList components = version->versionId().split('.');
    if (components.count() == 4) {
        bool thirdOk = false;
        bool fourthOk = false;
        const int third = components.at(2).toInt(&thirdOk);
        const int fourth = components.at(3).toInt(&fourthOk);
        if (thirdOk && fourthOk) {
            return QStringLiteral("%1.%2.%3.0").arg(components.at(0), components.at(1)).arg(third * 100 + fourth);
        }
    }
    return QString();
}

void BedrockInstallTask::executeTask()
{
    if (!m_version) {
        emitFailed(tr("This Bedrock version is not known to the version list. Refresh it from the instance's Version page and try again."));
        return;
    }
    if (m_version->packaging() == BedrockVersion::Packaging::Unknown) {
        emitFailed(tr("The packaging of Bedrock version %1 is unknown, so it cannot be installed.").arg(m_version->versionId()));
        return;
    }
#if defined(Q_OS_LINUX)
    if (m_version->packaging() == BedrockVersion::Packaging::Appx) {
        emitFailed(tr("Legacy UWP packages (.appx) are Windows-only. Pick a current GDK-era version instead."));
        return;
    }
#endif

    setStatus(tr("Preparing the Bedrock %1 game files").arg(m_version->versionId()));
    if (!FS::ensureFolderPathExists(BedrockPaths::packageDir(m_version->versionId()))) {
        emitFailed(tr("Could not create the package cache directory for Bedrock %1.").arg(m_version->versionId()));
        return;
    }

    m_finalPackagePath = packageCachePath(m_version);
    m_tempPackagePath = m_finalPackagePath + QStringLiteral(".part");
    m_pendingUrls = m_version->packageUrls();

    /* A previously downloaded, hash-verified package skips the network. */
    if (QFileInfo::exists(m_finalPackagePath)
        && (!m_version->hasSha256() || BedrockArtifactTask::verifyFileSha256(m_finalPackagePath, m_version->sha256()))) {
        runPlatformInstall();
        return;
    }

    beginPackageDownload();
}

void BedrockInstallTask::beginPackageDownload()
{
    if (m_pendingUrls.isEmpty()) {
        tryFe3Resolution();
        return;
    }

    m_currentUrl = m_pendingUrls.takeFirst();
    setStatus(tr("Downloading the Bedrock %1 game package").arg(m_version->versionId()));
    setDetails(m_currentUrl);

    m_netJob.reset(new NetJob(tr("Download Bedrock game package"), APPLICATION->network()));
    auto download = Net::Download::makeFile(QUrl(m_currentUrl), m_tempPackagePath);
    if (m_version->hasSha256()) {
        download->addValidator(new Net::ChecksumValidator(QCryptographicHash::Algorithm::Sha256, m_version->sha256()));
    }
    m_netJob->addNetAction(download);
    connect(m_netJob.get(), &Task::succeeded, this, &BedrockInstallTask::packageDownloadFinished);
    connect(m_netJob.get(), &Task::failed, this, [this](const QString& reason) { rotateMirror(reason); });
    connect(m_netJob.get(), &Task::progress, this, &Task::setProgress);
    connect(m_netJob.get(), &Task::stepProgress, this, &BedrockInstallTask::propagateStepProgress);
    connect(m_netJob.get(), &Task::status, this, &Task::setStatus);
    m_netJob->start();
}

void BedrockInstallTask::rotateMirror(const QString& reason)
{
    qWarning() << "Bedrock install: the download of" << m_currentUrl << "failed:" << reason;
    QFile::remove(m_tempPackagePath);
    if (!m_pendingUrls.isEmpty()) {
        beginPackageDownload();
        return;
    }
    tryFe3Resolution();
}

void BedrockInstallTask::tryFe3Resolution()
{
    if (m_fe3Attempted) {
        QString detail;
        if (!m_fe3Error.isEmpty()) {
            detail = QStringLiteral(" (") + m_fe3Error + QStringLiteral(")");
        }
        emitFailed(tr("No download location could be resolved for Bedrock %1. Every mirror of the version databases failed%2.")
                       .arg(m_version->versionId(), detail));
        return;
    }
    m_fe3Attempted = true;

    if (m_version->packaging() != BedrockVersion::Packaging::Appx || m_version->updateIds().isEmpty()) {
        emitFailed(tr("The version databases currently carry no direct download URL for Bedrock %1. Refresh the version list and try again.")
                       .arg(m_version->versionId()));
        return;
    }

    setStatus(tr("Resolving the package through the Microsoft delivery service"));
    const QList<LegacyUwpFe3Source::FileLocation> locations =
        LegacyUwpFe3Source::filesForUpdates(m_version->updateIds(), m_version->channel(), &m_fe3Error);
    for (const LegacyUwpFe3Source::FileLocation& location : locations) {
        if (location.url.startsWith(QLatin1String("http"))) {
            m_pendingUrls.append(location.url);
        }
    }
    if (m_pendingUrls.isEmpty()) {
        emitFailed(tr("The Microsoft delivery service did not resolve Bedrock %1: %2").arg(m_version->versionId(), m_fe3Error));
        return;
    }
    beginPackageDownload();
}

void BedrockInstallTask::packageDownloadFinished()
{
    /* The validator already checked the hash mid-download; check the finished file too. */
    if (m_version->hasSha256() && !BedrockArtifactTask::verifyFileSha256(m_tempPackagePath, m_version->sha256())) {
        rotateMirror(tr("the downloaded file did not match the known SHA-256"));
        return;
    }
    if (QFile::exists(m_finalPackagePath)) {
        QFile::remove(m_finalPackagePath);
    }
    if (!QFile::rename(m_tempPackagePath, m_finalPackagePath)) {
        emitFailed(tr("Could not finalize the game package download."));
        return;
    }
    runPlatformInstall();
}

void BedrockInstallTask::runPlatformInstall()
{
#if defined(Q_OS_WIN)

    if (m_version->packaging() == BedrockVersion::Packaging::Msixvc) {
        if (GdkPackageInstaller::isRunningUnderWine()) {
            emitFailed(tr("Bedrock Edition cannot be installed under Wine (Winlator, Proton, ...): it needs real Windows package "
                          "deployment and Gaming Services."));
            return;
        }
        /* The installed-package query runs off the UI thread; the rest continues in continueWindowsInstall(). */
        setStatus(tr("Checking the installed Bedrock game"));
        BedrockAsync::runOffThread<QString>(
            this, []() { return GdkPackageInstaller::installedMinecraftPackageFullName(); },
            [this](QString installed) { continueWindowsInstall(installed); });
        return;
    }

    if (m_version->packaging() == BedrockVersion::Packaging::Appx) {
        /*
         * The extract-and-register pipeline: the package lands inside the
         * instance, and the developer-mode registration happens at launch time,
         * when the instance folder is at its final location (a registered loose
         * package points into its folder).
         */
        const QString gameDir = FS::PathCombine(m_destinationDir, QStringLiteral("game"));
        if (!FS::ensureFolderPathExists(gameDir)) {
            emitFailed(tr("Could not create the game folder for the instance."));
            return;
        }
        setStatus(tr("Extracting the legacy UWP package"));
        const auto extracted = MMCZip::extractDir(m_finalPackagePath, gameDir);
        if (!extracted.has_value()) {
            emitFailed(tr("Could not extract the legacy UWP package of Bedrock %1.").arg(m_version->versionId()));
            return;
        }
        const QString manifestPath = FS::PathCombine(gameDir, QStringLiteral("AppxManifest.xml"));
        if (!QFileInfo::exists(manifestPath)) {
            emitFailed(tr("The package of Bedrock %1 carried no AppxManifest.xml, so it is not a legacy UWP package.")
                           .arg(m_version->versionId()));
            return;
        }
        QFile::remove(FS::PathCombine(gameDir, QStringLiteral("AppxSignature.p7x")));
        finishInstall(true);
        return;
    }

    emitFailed(tr("The packaging of Bedrock %1 is not supported by the Windows install pipeline.").arg(m_version->versionId()));

#elif defined(Q_OS_LINUX)

    runLinuxExtraction();

#else

    emitFailed(tr("Bedrock game installation is not supported on this platform (no macOS backend is planned)."));

#endif
}

void BedrockInstallTask::continueWindowsInstall(const QString& installed)
{
#if defined(Q_OS_WIN)
    /*
     * The Windows single-version constraint, made explicit before the shared
     * game is replaced: every Bedrock instance shares the installed package.
     */
    const QString expected = expectedWindowsPackageVersion(m_version);
    if (!installed.isEmpty() && !expected.isEmpty() && !installed.contains(expected)) {
        if (m_interactive) {
            const auto answer =
                QMessageBox::question(nullptr,
                                       tr("Replace the installed Bedrock game?"),
                                       tr("Bedrock %1 will replace the currently installed shared Bedrock game (%2).\n\n"
                                          "Only one GDK-era version of the game can be installed at a time: every Bedrock instance "
                                          "launches the installed one, and switching versions reinstalls it. Continue?")
                                           .arg(m_version->versionId(), installed),
                                       QMessageBox::Yes | QMessageBox::No,
                                       QMessageBox::No);
            if (answer != QMessageBox::Yes) {
                emitFailed(tr("The game install was cancelled: the currently installed Bedrock version was kept."));
                return;
            }
        }
    }

    setStatus(tr("Installing the game with Windows (this is the entitled install; the account must own the game)"));
    const QString packagePath = m_finalPackagePath;
    /* Add-AppxPackage can take minutes: never on the UI thread. */
    BedrockAsync::runOffThread<QString>(
        this,
        [packagePath]() {
            QString error;
            return GdkPackageInstaller::installPackageFile(packagePath, &error) ? QString() : (error.isEmpty() ? QStringLiteral("unknown error") : error);
        },
        [this](QString error) {
            if (!error.isEmpty()) {
                emitFailed(tr("Windows refused to install the Bedrock %1 package: %2").arg(m_version->versionId(), error));
                return;
            }
            finishInstall(false);
        });
#else
    Q_UNUSED(installed)
#endif
}

void BedrockInstallTask::runLinuxExtraction()
{
    /*
     * The encrypted GDK package needs the licensing client's key to extract;
     * when the client is not set up yet, the staged package is still useful
     * and the first launch picks the extraction up from here.
     */
    QString reason;
    const QString binary = XodusLicensingClient::get()->resolveBinary(QString(), &reason);
    if (binary.isEmpty()) {
        qWarning() << "Bedrock install: the licensing client is not available yet, the package stays staged:" << reason;
        setDetails(reason);
        finishInstall(false);
        return;
    }

    const QString gameDir = FS::PathCombine(m_destinationDir, QStringLiteral("game"));
    if (!FS::ensureFolderPathExists(gameDir)) {
        emitFailed(tr("Could not create the game folder for the instance."));
        return;
    }

    setStatus(tr("Extracting the game with the licensing client"));
    m_extractProcess = new QProcess(this);
    m_extractProcess->setProgram(binary);
    m_extractProcess->setArguments({ QStringLiteral("extract"), m_finalPackagePath, gameDir });
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("HOME"), BedrockPaths::xodusHomeDir());
    m_extractProcess->setProcessEnvironment(environment);
    connect(m_extractProcess, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus exitStatus) {
        linuxExtractionFinished(exitCode, static_cast<int>(exitStatus));
    });
    connect(m_extractProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            qWarning() << "Bedrock install: the licensing client could not be started for the extraction.";
            finishInstall(false);
        }
    });
    m_extractProcess->start();
}

void BedrockInstallTask::linuxExtractionFinished(int exitCode, int exitStatus)
{
    Q_UNUSED(exitStatus)
    finishInstall(exitCode == 0);
}

void BedrockInstallTask::finishInstall(bool extracted)
{
    writeInstallMarker(extracted);
    setStatus(tr("Bedrock %1 is %2").arg(m_version->versionId(), extracted ? tr("installed") : tr("staged for the first launch")));
    emitSucceeded();
}

void BedrockInstallTask::writeInstallMarker(bool extracted)
{
    if (m_destinationDir.isEmpty()) {
        return;
    }
    const QString gameDir = FS::PathCombine(m_destinationDir, QStringLiteral("game"));
    if (!FS::ensureFolderPathExists(gameDir)) {
        return;
    }
    QFile marker(FS::PathCombine(gameDir, kMarkerFileName));
    if (!marker.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }
    QTextStream stream(&marker);
    stream << "version=" << m_version->versionId() << "\n";
    stream << "packaging=" << (m_version->packaging() == BedrockVersion::Packaging::Msixvc ? QStringLiteral("msixvc") : QStringLiteral("appx"))
           << "\n";
    stream << "extracted=" << (extracted ? QStringLiteral("true") : QStringLiteral("false")) << "\n";
    stream << "sha256=" << m_version->sha256() << "\n";
    marker.close();
}

bool BedrockInstallTask::isGameInstalled(const QString& destinationDir, const BedrockVersion::Ptr& version)
{
    if (!version || destinationDir.isEmpty()) {
        return false;
    }
    QFile marker(FS::PathCombine(FS::PathCombine(destinationDir, QStringLiteral("game")), kMarkerFileName));
    if (!marker.open(QIODevice::ReadOnly)) {
        return false;
    }
    QMap<QString, QString> fields;
    QTextStream stream(&marker);
    while (!stream.atEnd()) {
        const QString line = stream.readLine();
        const int split = line.indexOf('=');
        if (split > 0) {
            fields.insert(line.left(split), line.mid(split + 1));
        }
    }
    marker.close();

    if (fields.value(QStringLiteral("version")) != version->versionId()) {
        return false;
    }

#if defined(Q_OS_WIN)
    /*
     * On Windows the game is the package installed for the user (the GDK
     * system install, or the developer-mode registration of the legacy
     * pipeline): its full name carries the expected package version.
     */
    const QString installed = GdkPackageInstaller::installedMinecraftPackageFullName();
    const QString expected = expectedWindowsPackageVersion(version);
    return !installed.isEmpty() && !expected.isEmpty() && installed.contains(expected);
#else
    return fields.value(QStringLiteral("extracted")) == QLatin1String("true");
#endif
}
