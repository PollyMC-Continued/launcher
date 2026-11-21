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

#include "BedrockLaunchTask.h"

#include <QDebug>

std::unique_ptr<BedrockLaunchTask> BedrockLaunchTask::create(BedrockInstance* inst)
{
    // Mirrors LaunchTask::create(): constructs the task and marks the instance as
    // running before any step executes.
    auto task = std::unique_ptr<BedrockLaunchTask>(new BedrockLaunchTask(inst));
    task->init();
    return task;
}

BedrockLaunchTask::BedrockLaunchTask(BedrockInstance* instance) : LaunchTask(instance), m_bedrockInstance(instance) {}

void BedrockLaunchTask::executeTask()
{
    // Scaffolding state: the task machinery (step queue, logging, ownership,
    // running-state bookkeeping) is wired up, but no Windows GDK/UWP or Linux
    // Proton backend exists yet. Fail cleanly instead of pretending to launch.
    qWarning() << "BedrockLaunchTask: no Bedrock launch backend is implemented yet (instance"
               << (m_bedrockInstance ? m_bedrockInstance->id() : QString()) << ")";
    emitFailed(tr("Launching Bedrock instances is not implemented yet; the Windows and Linux backends arrive in the next steps of this update."));
}
