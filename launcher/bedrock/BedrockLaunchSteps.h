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

#include <QStringList>

#include "BedrockInstance.h"
#include "BedrockVersion.h"
#include "LoggedProcess.h"
#include "launch/LaunchStep.h"
#include "tasks/Task.h"

/*
 * The launch pipeline steps of Bedrock support, appended by
 * BedrockInstance::createLaunchTask() after the pre-flight check:
 *
 *  - BedrockEnsureGameStep: guarantees the chosen version's game is installed
 *    (running the install pipeline when it is not), registers legacy UWP
 *    packages once their folder is at its final location, and deploys the
 *    optional Windows launch helper that pins the exact GDK version.
 *  - BedrockWindowsLaunchStep: activates the installed game (GDK packages
 *    through their AppUserModelID, legacy UWP packages through the
 *    minecraft: protocol).
 *  - BedrockLinuxLaunchStep: the experimental GDK-Proton backend - the
 *    runtime setup (engine, umu, licensing client), the Vulkan 1.3 check, and
 *    the game process through the licensing client under the engine.
 */
class BedrockEnsureGameStep : public LaunchStep {
    Q_OBJECT
   public:
    explicit BedrockEnsureGameStep(LaunchTask* parent, BedrockInstance* instance);
    void executeTask() override;
    bool abort() override;

   private:
    void runInstall();
    void installSucceeded();
    void installFailed(const QString& reason);
    void registerLegacyUwpPackage(const BedrockVersion::Ptr& version);
    void deployWindowsLaunchHelper(const BedrockVersion::Ptr& version);

    BedrockInstance* m_bedrockInstance = nullptr;
    Task::Ptr m_installTask;
    bool m_aborted = false;
};

class BedrockWindowsLaunchStep : public LaunchStep {
    Q_OBJECT
   public:
    explicit BedrockWindowsLaunchStep(LaunchTask* parent, BedrockInstance* instance);
    void executeTask() override;

   private:
    /*
     * Links the game's data folder to this instance's own data folder (the
     * BedrockLauncher-proven model): the launched instance's data - worlds,
     * packs, settings - is the active one while it runs. A no-op off Windows
     * and when the instance setting for separate game data is off.
     */
    void redirectGameData(const BedrockVersion::Ptr& version);

    BedrockInstance* m_bedrockInstance = nullptr;
};

class BedrockLinuxLaunchStep : public LaunchStep {
    Q_OBJECT
   public:
    explicit BedrockLinuxLaunchStep(LaunchTask* parent, BedrockInstance* instance);
    void executeTask() override;
    bool abort() override;

   private slots:
    void setupFinished();
    void setupFailed(const QString& reason);
    void gameLogLines(const QStringList& lines, MessageLevel level);
    void gameProcessStateChanged(LoggedProcess::State state);

   private:
    void runSetup();
    void startGameProcess();
    bool checkVulkan();
    void writeRegistryToken();

    BedrockInstance* m_bedrockInstance = nullptr;
    Task::Ptr m_setupTask;
    LoggedProcess m_gameProcess;
    bool m_aborted = false;
};
