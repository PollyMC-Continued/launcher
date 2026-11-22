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

#include "BedrockPreflightStep.h"

#include "BedrockVersionList.h"
#include "MessageLevel.h"
#include "launch/LaunchTask.h"

BedrockPreflightStep::BedrockPreflightStep(LaunchTask* parent, BedrockInstance* instance)
    : LaunchStep(parent), m_instance(instance)
{}

void BedrockPreflightStep::executeTask()
{
    if (!m_instance) {
        emitFailed(tr("Internal error: the Bedrock pre-flight step has no instance."));
        return;
    }

    QStringList lines;
    lines << tr("Bedrock pre-flight check:");

    const QString versionId = m_instance->bedrockVersion();
    if (versionId.isEmpty()) {
        lines << tr("  Game version: none selected.");
        lines << tr("  Open this instance's Version page and pick a Bedrock version first.");
        emit logLines(lines, MessageLevel::Error);
        emitFailed(tr("This Bedrock instance has no game version selected. Pick one on the instance's Version page."));
        return;
    }

    lines << tr("  Game version: %1").arg(versionId);

    const BedrockVersion::Ptr version = BedrockVersionList::get()->findBedrockVersion(versionId);
    if (version) {
        lines << tr("  Channel: %1").arg(BedrockVersion::channelToString(version->channel()));
        lines << tr("  Packaging: %1").arg(version->typeString());
        lines << tr("  Package URLs known: %1")
                     .arg(version->hasPackageUrls() ? tr("yes") : tr("no (resolved when the install pipeline runs)"));
    } else {
        lines << tr("  Details unknown: the Bedrock version list is not loaded yet.");
        lines << tr("  The next step will try to install the game anyway; open the instance's Version page once while online "
                    "if this fails.");
    }

#if defined(Q_OS_WIN)
    if (version) {
        if (version->isLegacyUwp()) {
            lines << tr("  Windows pipeline: legacy UWP package (.appx) - download, hash check, extract, developer-mode "
                        "registration, protocol launch.");
        } else {
            lines << tr("  Windows pipeline: GDK package (.msixvc) - download, hash check, entitled install through "
                        "Windows, AppUserModelID launch.");
        }
        lines << tr("  The installed game is shared between Bedrock instances; this instance's game data folder is linked "
                    "to it on launch.");
    }
    emit logLines(lines, MessageLevel::Launcher);
    emitSucceeded();
#elif defined(Q_OS_LINUX)
    if (version && version->packaging() == BedrockVersion::Packaging::Appx) {
        emit logLines(lines, MessageLevel::Error);
        emitFailed(tr("Legacy UWP packages (.appx) are Windows-only. Pick a current GDK-era version instead."));
        return;
    }
    lines << tr("  Linux pipeline: GDK-Proton backend (experimental) - the engine, umu and the licensing client are "
                "fetched on first use (hash-pinned), then the game runs through them.");
    lines << tr("  Requires a Vulkan 1.3-class driver and internet at every game start; achievements do not unlock.");
    emit logLines(lines, MessageLevel::Launcher);
    emitSucceeded();
#else
    emit logLines(lines, MessageLevel::Launcher);
    emitFailed(tr("Bedrock support has no backend on this platform: the Windows backend needs Windows, the Linux backend "
                  "needs Linux, and no macOS backend is planned."));
#endif
}

bool BedrockPreflightStep::abort()
{
    emitAborted();
    return true;
}
