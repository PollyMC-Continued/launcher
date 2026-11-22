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

#include "BedrockVersion.h"
#include "InstanceTask.h"

/*
 * Creates a new Bedrock instance from the Bedrock page of the New Instance
 * dialog.
 *
 * Mirrors the Java creation tasks' flow: the task fills the staging folder that
 * InstanceList::wrapInstanceTask prepared (instance.cfg with the instance type,
 * the name, the icon and the chosen Bedrock version), and - when the Bedrock
 * page's "Download the game files on creation" switch is on - runs the install
 * pipeline for the game package right after (downloading it into the shared
 * package cache and running the platform install step). Committing the folder
 * into the instance list happens in InstanceList::wrapInstanceTask when the
 * task succeeds.
 */
class BedrockCreationTask final : public InstanceTask {
    Q_OBJECT
   public:
    explicit BedrockCreationTask(BedrockVersion::Ptr version, bool installGame);
    ~BedrockCreationTask() override = default;

   protected:
    void executeTask() override;

   private:
    void runInstall(const QString& instanceDir);

    /* The Bedrock game version the instance starts with ("1.21.120.20"). */
    BedrockVersion::Ptr m_version;
    /* Whether the game package is installed during creation. */
    bool m_installGame = false;
    Task::Ptr m_installTask;
};
