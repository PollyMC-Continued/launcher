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

#include "BedrockInstance.h"

#include <QDebug>
#include <QDir>
#include <QMap>
#include <QSet>

#include "BedrockInstallTask.h"
#include "BedrockLaunchSteps.h"
#include "BedrockLaunchTask.h"
#include "BedrockPreflightStep.h"
#include "BedrockVersionList.h"
#include "FileSystem.h"
#include "GdkPackageInstaller.h"
#include "GdkProtonEngineProvider.h"
#include "launch/LaunchTask.h"

BedrockInstance::BedrockInstance(SettingsObject* globalSettings, std::unique_ptr<SettingsObject> settings, const QString& rootDir)
    : BaseInstance(globalSettings, std::move(settings), rootDir)
{
}

void BedrockInstance::saveNow()
{
    // Instance settings persist through the settings object; there is nothing
    // component-specific to flush (unlike MinecraftInstance's PackProfile).
}

void BedrockInstance::loadSpecificSettings()
{
    if (isSpecificSettingsLoaded())
        return;

    // The Bedrock game version chosen for this instance.
    m_settings->registerSetting("BedrockVersion", "");

    /*
     * Backend settings (all optional):
     *  - BedrockSeparateGameData (Windows, default on): keep this instance's
     *    game data (worlds, packs, settings) in its own folder and link the
     *    game to it on launch, instead of sharing the game's default folder.
     *  - BedrockXodusPath: a locally built xodus-cli for the Linux backend,
     *    instead of the hash-pinned managed copy.
     *  - BedrockGdkLaunchHelperPath: BedrockLauncher's gamelaunchhelper.dll,
     *    pinning the exact installed GDK version on Windows.
     *  - BedrockMsaRefreshToken: advanced; written into the WineGDK registry
     *    of the instance prefix for the in-game Xbox sign-in.
     *  - BedrockLinuxLaunchCommand: advanced; the full launch command of the
     *    experimental Linux backend (placeholders {umu_run} {engine} {prefix}
     *    {xodus} {game_dir}).
     */
    m_settings->registerSetting("BedrockSeparateGameData", true);
    m_settings->registerSetting("BedrockXodusPath", "");
    m_settings->registerSetting("BedrockGdkLaunchHelperPath", "");
    m_settings->registerSetting("BedrockMsaRefreshToken", "");
    m_settings->registerSetting("BedrockLinuxLaunchCommand", "");

    setSpecificSettingsLoaded(true);
}

QString BedrockInstance::typeName() const
{
    return "Bedrock";
}

QSet<QString> BedrockInstance::traits() const
{
    // Shared code (LaunchController, InstancePageProvider, ...) uses this trait to
    // tell Bedrock instances apart from Java ones without hardcoding type strings.
    return { "bedrock" };
}

void BedrockInstance::populateLaunchMenu(QMenu* menu)
{
    Q_UNUSED(menu)
    // Bedrock has a single launch mode: the platform backend picked by the
    // launch pipeline (Windows GDK / legacy UWP activation, Linux GDK-Proton).
}

QString BedrockInstance::modsRoot() const
{
    // Bedrock has no Java-style mods folder; resource/behavior pack folders are
    // managed as part of Phase 2 content support.
    return QString();
}

QString BedrockInstance::instanceConfigFolder() const
{
    return instanceRoot();
}

QString BedrockInstance::bedrockVersion()
{
    return settings()->get("BedrockVersion").toString();
}

void BedrockInstance::setBedrockVersion(const QString& version)
{
    settings()->set("BedrockVersion", version);
    // The version is part of the instance's statusbar description, so the
    // instance list needs to hear about the change.
    emit propertiesChanged(this);
}

QList<Task::Ptr> BedrockInstance::createUpdateTask()
{
    /* Install/update the game package of the chosen version (GDK install,
     * legacy UWP extract-and-register, Linux staging - the pipeline picks by
     * version and platform). */
    QList<Task::Ptr> tasks;
    tasks.append(BedrockInstallTask::create(this));
    return tasks;
}

LaunchTask* BedrockInstance::createLaunchTask(AuthSessionPtr account, MinecraftTarget::Ptr targetToJoin)
{
    Q_UNUSED(account)
    Q_UNUSED(targetToJoin)
    // The launcher's Java accounts and server-join targets do not apply to Bedrock:
    // the game itself handles Xbox sign-in on Windows, and the Linux licensing client
    // (Xodus) handles sign-in there.
    updateRuntimeContext();

    auto process = BedrockLaunchTask::create(this);
    /*
     * The pipeline: the pre-flight check prints what the launcher knows about
     * the instance's Bedrock version (and fails when none is selected), the
     * ensure step installs the game when it is missing, and the platform step
     * launches it (Windows: activation of the installed package; Linux: the
     * experimental GDK-Proton backend).
     */
    process->appendStep(makeShared<BedrockPreflightStep>(process.get(), this));
    process->appendStep(makeShared<BedrockEnsureGameStep>(process.get(), this));
#if defined(Q_OS_WIN)
    process->appendStep(makeShared<BedrockWindowsLaunchStep>(process.get(), this));
#elif defined(Q_OS_LINUX)
    process->appendStep(makeShared<BedrockLinuxLaunchStep>(process.get(), this));
#else
    /* Unsupported platforms never get here: the pre-flight check fails first. */
#endif
    m_launchProcess = std::move(process);
    emit launchTaskChanged(m_launchProcess.get());
    return m_launchProcess.get();
}

QProcessEnvironment BedrockInstance::createEnvironment()
{
    /* The platform launch steps build their own environment (the Linux backend's
     * WINEPREFIX/PROTONPATH set lives in GdkProtonEngineProvider). */
    return QProcessEnvironment();
}

QProcessEnvironment BedrockInstance::createLaunchEnvironment()
{
    // See createEnvironment().
    return QProcessEnvironment();
}

QStringList BedrockInstance::getLogFileSearchPaths()
{
    /* The Windows game logs land in the game's own data folders; the Linux
     * prefix mirrors them under the instance's proton-prefix. */
#if defined(Q_OS_LINUX)
    return { FS::PathCombine(instanceRoot(), QStringLiteral("proton-prefix")) };
#else
    return {};
#endif
}

QString BedrockInstance::getStatusbarDescription()
{
    QString description = tr("Bedrock instance");
    const auto version = bedrockVersion();
    if (!version.isEmpty()) {
        description += QString(" (%1)").arg(version);
    }
    return description;
}

QMap<QString, QString> BedrockInstance::getVariables()
{
    // Generic variables for custom commands; Java-specific ones (INST_JAVA etc.)
    // deliberately do not exist for Bedrock.
    QMap<QString, QString> out;
    out.insert("INST_NAME", name());
    out.insert("INST_ID", id());
    out.insert("INST_DIR", QDir::toNativeSeparators(QDir(instanceRoot()).absolutePath()));
    return out;
}

QStringList BedrockInstance::verboseDescription(AuthSessionPtr session, MinecraftTarget::Ptr targetToJoin)
{
    Q_UNUSED(session)
    Q_UNUSED(targetToJoin)
    QStringList out;
    out << "Bedrock instance";
    out << "  instance folder: " + instanceRoot();
    const auto version = bedrockVersion();
    out << "  game version: " + (version.isEmpty() ? QStringLiteral("(not set)") : version);
#if defined(Q_OS_WIN)
    out << "  backend: Windows - GDK packages through AddPackageAsync (entitled install), legacy UWP packages "
           "extract-and-register; launch through the package's AppUserModelID";
    out << "  game data: " + (settings()->get("BedrockSeparateGameData").toBool() ? QStringLiteral("per-instance (linked on launch)")
                                                                                    : QStringLiteral("shared with the game's default folder"));
#elif defined(Q_OS_LINUX)
    out << "  backend: Linux - GDK-Proton (experimental), per-instance prefix, Xodus licensing client, umu-run";
#else
    out << "  backend: none (this platform has no Bedrock backend)";
#endif
    return out;
}

void BedrockInstance::updateRuntimeContext()
{
    // NOOP for now.
}
