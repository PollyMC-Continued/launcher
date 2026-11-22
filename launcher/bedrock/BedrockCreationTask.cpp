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

#include "BedrockCreationTask.h"

#include <utility>

#include <memory>

#include "BedrockInstallTask.h"
#include "BedrockInstance.h"
#include "FileSystem.h"
#include "settings/INISettingsObject.h"
#include "settings/SettingsObject.h"

BedrockCreationTask::BedrockCreationTask(BedrockVersion::Ptr version, bool installGame)
    : m_version(std::move(version)), m_installGame(installGame)
{}

void BedrockCreationTask::executeTask()
{
    setStatus(tr("Creating Bedrock instance"));

    /*
     * The staging folder prepared by InstanceList::wrapInstanceTask becomes the
     * instance root. instance.cfg is written through the settings object, exactly
     * like the Java creation tasks: BaseInstance's constructor registers the
     * common keys (name, iconKey, InstanceType) and BedrockInstance's
     * loadSpecificSettings() registers the Bedrock ones, so every value below
     * lands in the file. The settings object persists on each write, so the
     * staging folder is complete when this part succeeds.
     */
    auto instance = std::make_unique<BedrockInstance>(m_globalSettings,
                                                     std::make_unique<INISettingsObject>(FS::PathCombine(m_stagingPath, "instance.cfg")),
                                                     m_stagingPath);
    {
        SettingsObject::Lock lock(instance->settings());
        instance->settings()->set("InstanceType", "Bedrock");
        instance->setName(name());
        instance->setIconKey(m_instIcon);
        instance->setBedrockVersion(m_version ? m_version->versionId() : QString());
    }

    if (!m_installGame || !m_version) {
        emitSucceeded();
        return;
    }
    runInstall(m_stagingPath);
}

void BedrockCreationTask::runInstall(const QString& instanceDir)
{
    setStatus(tr("Installing the Bedrock %1 game package").arg(m_version->versionId()));
    m_installTask = makeShared<BedrockInstallTask>(instanceDir, m_version);
    connect(m_installTask.get(), &Task::succeeded, this, [this]() { emitSucceeded(); });
    connect(m_installTask.get(), &Task::failed, this, [this](const QString& reason) { emitFailed(reason); });
    connect(m_installTask.get(), &Task::progress, this, &Task::setProgress);
    connect(m_installTask.get(), &Task::stepProgress, this, &BedrockCreationTask::propagateStepProgress);
    connect(m_installTask.get(), &Task::status, this, &Task::setStatus);
    m_installTask->start();
}
