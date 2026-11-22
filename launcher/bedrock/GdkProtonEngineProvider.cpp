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

#include "GdkProtonEngineProvider.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include "Application.h"
#include "BedrockArtifactTask.h"
#include "FileSystem.h"
#include "tasks/SequentialTask.h"

GdkProtonEngineProvider::GdkProtonEngineProvider(QObject* parent) : QObject(parent), EngineProvider() {}

GdkProtonEngineProvider* GdkProtonEngineProvider::get()
{
    static GdkProtonEngineProvider* globalProvider = new GdkProtonEngineProvider(APPLICATION);
    return globalProvider;
}

QString GdkProtonEngineProvider::id() const
{
    return QStringLiteral("gdk-proton");
}

QString GdkProtonEngineProvider::displayName() const
{
    return QStringLiteral("GDK-Proton (experimental)");
}

QString GdkProtonEngineProvider::engineRoot() const
{
    /*
     * The pinned engine unpacks under its own name; the upstream layout name
     * is accepted as well so a hand-placed engine copy works without the
     * launcher's own archive being around.
     */
    const QString managed = FS::PathCombine(BedrockPaths::enginesDir(), BedrockArtifacts::winegdkEngineDirName());
    if (QFileInfo::exists(managed)) {
        return managed;
    }
    const QString upstream = FS::PathCombine(BedrockPaths::enginesDir(), QStringLiteral("GDK-Proton-xuser"));
    if (QFileInfo::exists(upstream)) {
        return upstream;
    }
    return QString();
}

QString GdkProtonEngineProvider::umuRunPath() const
{
    /*
     * The pinned zipapp unpacks its top-level "umu/" directory into the umu
     * dir, and the entry point lives inside it: <umuDir>/umu/umu-run. That is
     * also the directory umu itself computes as its local folder from
     * UMU_FOLDERS_PATH (umu appends "umu" to it), so the Steam Linux Runtime
     * umu fetches on first use lands next to the entry point - the same
     * arrangement the reference BedrockOnLinux layout uses.
     */
    return FS::PathCombine(BedrockPaths::umuDir(), BedrockArtifacts::umuRunDirName(), BedrockArtifacts::umuRunName());
}

QString GdkProtonEngineProvider::gdkDepsDir() const
{
    return BedrockPaths::gdkDepsDir();
}

bool GdkProtonEngineProvider::isInstalled() const
{
    if (engineRoot().isEmpty()) {
        return false;
    }
    if (!BedrockArtifactTask::verifyFileSha256(umuRunPath(), BedrockArtifacts::umuRunSha256())) {
        return false;
    }
    if (!BedrockArtifactTask::isArtifactPresent(BedrockPaths::gdkDepsDir(), BedrockArtifacts::gdkDependency(QStringLiteral("libHttpClient.GDK.dll")))) {
        return false;
    }
    if (!BedrockArtifactTask::isArtifactPresent(BedrockPaths::gdkDepsDir(), BedrockArtifacts::gdkDependency(QStringLiteral("XCurl.dll")))) {
        return false;
    }
    if (!QFileInfo::exists(FS::PathCombine(BedrockPaths::gdkDepsDir(), BedrockArtifacts::caBundle().fileName))) {
        return false;
    }
    if (!BedrockArtifactTask::isArtifactPresent(BedrockPaths::opensslXcurlDir(), BedrockArtifacts::opensslXcurlSet())) {
        return false;
    }
    return true;
}

Task::Ptr GdkProtonEngineProvider::artifactTask(const BedrockArtifacts::Artifact& artifact,
                                                const QString& destinationDir,
                                                bool extract,
                                                const QString& expectedInnerFile,
                                                bool executable)
{
    BedrockArtifactTask::Options options{};
    options.artifact = artifact;
    options.destinationDir = destinationDir;
    options.extract = extract;
    options.expectedInnerFile = expectedInnerFile;
    options.executable = executable;
    return makeShared<BedrockArtifactTask>(options);
}

Task::Ptr GdkProtonEngineProvider::ensureInstalled()
{
    /*
     * A corrupt or stale umu-run would keep the marker-valid early exit of the
     * artifact task from ever fixing it; drop the markers so the zipapp is
     * genuinely fetched again in that case.
     */
    if (!BedrockArtifactTask::verifyFileSha256(umuRunPath(), BedrockArtifacts::umuRunSha256())) {
        const QString archiveName = BedrockArtifacts::umuZipapp().fileName;
        QFile::remove(FS::PathCombine(BedrockPaths::umuDir(), QStringLiteral(".verified-") + archiveName));
        QFile::remove(FS::PathCombine(BedrockPaths::umuDir(), archiveName));
    }

    auto sequence = makeShared<SequentialTask>(tr("GDK-Proton engine setup"));

    sequence->addTask(artifactTask(BedrockArtifacts::umuZipapp(), BedrockPaths::umuDir(), true, BedrockArtifacts::umuRunName(), true));
    sequence->addTask(artifactTask(BedrockArtifacts::winegdkEngine(), BedrockPaths::enginesDir(), true, QString(), false));
    sequence->addTask(artifactTask(BedrockArtifacts::gdkDependency(QStringLiteral("libHttpClient.GDK.dll")), BedrockPaths::gdkDepsDir(), false, QString(), false));
    sequence->addTask(artifactTask(BedrockArtifacts::gdkDependency(QStringLiteral("XCurl.dll")), BedrockPaths::gdkDepsDir(), false, QString(), false));
    sequence->addTask(artifactTask(BedrockArtifacts::caBundle(), BedrockPaths::gdkDepsDir(), false, QString(), false));
    sequence->addTask(artifactTask(BedrockArtifacts::opensslXcurlSet(), BedrockPaths::opensslXcurlDir(), true, QString(), false));

    return sequence;
}

QProcessEnvironment GdkProtonEngineProvider::launchEnvironment(const QString& engineRoot,
                                                               const QString& winePrefix,
                                                               const QString& xodusHome,
                                                               const QString& gameDir)
{
    /*
     * The verified recipe's environment for the umu-run invocation: PROTONPATH
     * to the managed engine, WINEPREFIX to the per-instance prefix, the fixed
     * umu variables, and the isolated HOME for the licensing client so its
     * token keyring persists. The game data directory is exported for the
     * stack's own tooling to find.
     */
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("WINEPREFIX"), winePrefix);
    environment.insert(QStringLiteral("PROTONPATH"), engineRoot);
    environment.insert(QStringLiteral("PROTON_VERB"), QStringLiteral("run"));
    environment.insert(QStringLiteral("GAMEID"), QStringLiteral("0"));
    environment.insert(QStringLiteral("STORE"), QStringLiteral("none"));
    environment.insert(QStringLiteral("HOME"), xodusHome);
    environment.insert(QStringLiteral("UMU_FOLDERS_PATH"), BedrockPaths::umuDir());
    environment.insert(QStringLiteral("BEDROCK_GAME_DIR"), gameDir);
    return environment;
}
