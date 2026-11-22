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

#pragma once

#include <QString>
#include <QStringList>

#include "BedrockVersion.h"
#include "net/NetJob.h"
#include "tasks/Task.h"

class BedrockInstance;
class QProcess;

/*
 * The unified game install/update pipeline behind both Windows packaging
 * generations and the Linux staging, one task so the version list picks the
 * pipeline by version and the UI never shows the difference:
 *
 *  - current GDK packages (.msixvc): downloaded from the Xbox CDN URLs the
 *    version databases carry (mirror rotation, SHA-256 verified) into the
 *    shared package cache, then - on Windows - handed to
 *    PackageManager.AddPackageAsync so Gaming Services performs the entitled
 *    install; on Linux the encrypted package is staged for the licensing
 *    client to extract.
 *  - legacy UWP packages (.appx, up to 1.21.120.20): when the version
 *    databases carry no direct URLs, the fe3 delivery service resolves the
 *    download locations; the package is hash-verified, extracted into the
 *    instance's game folder, the signature file is dropped, and the folder is
 *    registered in developer mode (at launch time, once the instance folder is
 *    committed to its final location).
 *
 * Nothing here bypasses the user's entitlement: on Windows the install goes
 * through Windows itself, and on Linux the licensing client handles the
 * account side. No game files are bundled or re-hosted.
 */
class BedrockInstallTask : public Task {
    Q_OBJECT
   public:
    /*
     * Install/update the game of an existing instance (its chosen version;
     * the version list must know it). The returned task is not started.
     */
    static Task::Ptr create(BedrockInstance* instance);

    /*
     * Install the given version into destinationDir (an instance root or the
     * creation dialog's staging folder); the game data lands in
     * destinationDir/game. The returned task is not started.
     */
    explicit BedrockInstallTask(const QString& destinationDir, BedrockVersion::Ptr version);

    ~BedrockInstallTask() override;

    void executeTask() override;

    ////// state queries (used by the launch ensure step) //////

    /* The shared package cache path of a version. */
    static QString packageCachePath(const BedrockVersion::Ptr& version);

    /* True when the version's game data is in place under destinationDir/game. */
    static bool isGameInstalled(const QString& destinationDir, const BedrockVersion::Ptr& version);

    /*
     * The Windows package version string of a Bedrock version id
     * ("1.26.65.1" -> "1.26.6501.0"), read from the package file name the
     * databases carry, computed from the id when there is no file name.
     */
    static QString expectedWindowsPackageVersion(const BedrockVersion::Ptr& version);

   private:
    void beginPackageDownload();
    void rotateMirror(const QString& reason);
    void tryFe3Resolution();
    void packageDownloadFinished();
    void runPlatformInstall();
    void continueWindowsInstall(const QString& installed);
    void runLinuxExtraction();
    void linuxExtractionFinished(int exitCode, int exitStatus);
    void finishInstall(bool extracted);

    void writeInstallMarker(bool extracted);

    BedrockVersion::Ptr m_version;
    QString m_destinationDir;
    NetJob::Ptr m_netJob;
    QProcess* m_extractProcess = nullptr;
    QString m_tempPackagePath;
    QString m_finalPackagePath;
    QStringList m_pendingUrls;
    QString m_currentUrl;
    QString m_fe3Error;
    bool m_fe3Attempted = false;
    bool m_interactive = true;
};
