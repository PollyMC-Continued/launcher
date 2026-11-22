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

#include "XodusLicensingClient.h"

#include <QDir>
#include <QFileInfo>

#include "Application.h"
#include "BedrockArtifactTask.h"

XodusLicensingClient::XodusLicensingClient(QObject* parent) : QObject(parent), LicensingClient() {}

XodusLicensingClient* XodusLicensingClient::get()
{
    static XodusLicensingClient* globalClient = new XodusLicensingClient(APPLICATION);
    return globalClient;
}

QString XodusLicensingClient::id() const
{
    return QStringLiteral("xodus");
}

QString XodusLicensingClient::displayName() const
{
    return QStringLiteral("Xodus (external licensing client, GPL-3)");
}

QString XodusLicensingClient::xodusHomePath()
{
    return BedrockPaths::xodusHomeDir();
}

QString XodusLicensingClient::resolveBinary(const QString& userOverride, QString* reason)
{
    /* A user-supplied binary always wins over the managed copy. */
    if (!userOverride.isEmpty()) {
        const QFileInfo overrideInfo(userOverride);
        if (overrideInfo.isExecutable()) {
            return overrideInfo.absoluteFilePath();
        }
        if (reason) {
            *reason = tr("The configured Xodus path (\"%1\") is not an executable file.").arg(userOverride);
        }
        return QString();
    }

    if (hasManagedCopy()) {
        return QFileInfo(QDir(BedrockPaths::xodusDir()), BedrockArtifacts::xodusBinaryName()).absoluteFilePath();
    }

    if (reason) {
        *reason = tr(
            "The Xodus licensing client is not set up yet. It is downloaded (hash-pinned) on the first launch that needs it, or "
            "set the instance's \"Xodus executable\" setting to a locally built xodus-cli.");
    }
    return QString();
}

bool XodusLicensingClient::hasManagedCopy() const
{
    const QFileInfo binary(QDir(BedrockPaths::xodusDir()), BedrockArtifacts::xodusBinaryName());
    if (!binary.exists() || !binary.isExecutable()) {
        return false;
    }
    return BedrockArtifactTask::isArtifactPresent(BedrockPaths::xodusDir(), BedrockArtifacts::xodusClient());
}

Task::Ptr XodusLicensingClient::ensureManagedCopy()
{
    /*
     * The caller starts and waits for the returned task. The task itself
     * succeeds immediately when the managed copy is already verified.
     */
    BedrockArtifactTask::Options options{};
    options.artifact = BedrockArtifacts::xodusClient();
    options.destinationDir = BedrockPaths::xodusDir();
    options.extract = true;
    options.expectedInnerFile = BedrockArtifacts::xodusBinaryName();
    options.executable = true;
    return makeShared<BedrockArtifactTask>(options);
}
