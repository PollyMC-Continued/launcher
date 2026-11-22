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

#include <QDir>

#include "BedrockArtifacts.h"
#include "net/NetJob.h"
#include "tasks/Task.h"

class QProcess;

/*
 * Downloads one pinned runtime artifact of Bedrock support: the engine, the
 * umu zipapp, the licensing client, the GDK dependency DLLs, the Windows
 * launch helper.
 *
 * Behavior follows the Phase 0 rule that keeps the launcher lightweight and
 * honest about third-party bytes: every artifact is fetched from its upstream
 * release at runtime, its SHA-256 is checked against the pin, and a mismatch
 * is refused rather than used. Direct-URL artifacts are downloaded from the
 * pinned URL; artifacts without a direct URL are resolved through the GitHub
 * releases API of their upstream repository (the pin, not the listing, is the
 * security boundary). Nothing is bundled with the launcher and nothing is
 * re-hosted.
 *
 * A ".verified" marker file next to the artifact records the verified hash,
 * so later runs can skip the work.
 */
class BedrockArtifactTask : public Task {
    Q_OBJECT
   public:
    struct Options {
        /* The artifact (name, url/repo, file name, pinned hash, license). */
        BedrockArtifacts::Artifact artifact;
        /* Directory the archive is stored (and, when extracting, unpacked) in. */
        QString destinationDir;
        /* Unpack the archive after download. */
        bool extract = true;
        /* Name of a file that must exist somewhere below destinationDir after
         * extraction (nullptr-check used as the success criterion). */
        QString expectedInnerFile;
        /* Make the expected inner file executable (Linux tool binaries). */
        bool executable = false;
    };

    explicit BedrockArtifactTask(Options options);
    ~BedrockArtifactTask() override;

    void executeTask() override;

    ////// shared helpers (used by the other Bedrock components too) //////

    /* SHA-256 of a file as lowercase hex; empty on error. */
    static QString fileSha256(const QString& path);

    /* True when the file's SHA-256 matches the expected hex string. */
    static bool verifyFileSha256(const QString& path, const QString& expectedHex);

    /*
     * True when destinationDir holds the artifact (its fileName exists, and
     * either no hash was pinned or a matching .verified marker is there).
     */
    static bool isArtifactPresent(const QString& destinationDir, const BedrockArtifacts::Artifact& artifact);

   private:
    void beginResolve();
    void releasesListed();
    void startDownload(const QString& url);
    void downloadFinished();
    void extractArchive();
    void extractionFinished(int exitCode, int exitStatus);
    void finishExtraction();

    Options m_options;
    NetJob::Ptr m_job;
    QProcess* m_extractProcess = nullptr;
    /* Current page while walking the GitHub releases listing (1-based). */
    int m_releasePage = 1;
    QString m_archivePath;
    QString m_tempPath;
    QString m_resolvedUrl;
    QByteArray m_releasesData;
};
