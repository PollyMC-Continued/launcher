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

#include "BedrockLaunchTask.h"
#include "BedrockPreflightStep.h"
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

    // The Bedrock game version chosen for this instance. The version list and the
    // download/install pipelines arrive with the later steps of Bedrock support.
    m_settings->registerSetting("BedrockVersion", "");

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
    // Bedrock has a single launch mode for now. The Windows GDK/UWP pipeline and the
    // Linux Proton pipeline may add entries here when they land.
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
    // TODO(install steps): the game install/update tasks for the selected Bedrock
    // version (GDK, then legacy UWP). The version list load itself lives in
    // BedrockVersionList and runs from the Version page / pre-flight step.
    return {};
}

LaunchTask* BedrockInstance::createLaunchTask(AuthSessionPtr account, MinecraftTarget::Ptr targetToJoin)
{
    Q_UNUSED(account)
    Q_UNUSED(targetToJoin)
    // The launcher's Java accounts and server-join targets do not apply to Bedrock:
    // the game itself handles Xbox sign-in on Windows, and the Linux licensing client
    // (a later step) handles sign-in there.
    updateRuntimeContext();

    auto process = BedrockLaunchTask::create(this);
    /*
     * The pre-flight step prints what the launcher knows about the instance's
     * Bedrock version into the instance console and (until the Windows install
     * pipeline lands) fails the launch with an explicit reason. The install and
     * launch steps of the GDK / legacy UWP backends slot in ahead of it.
     */
    process->appendStep(makeShared<BedrockPreflightStep>(process.get(), this));
    m_launchProcess = std::move(process);
    emit launchTaskChanged(m_launchProcess.get());
    return m_launchProcess.get();
}

QProcessEnvironment BedrockInstance::createEnvironment()
{
    // TODO(Windows/Linux backend steps): platform-specific environment for the
    // launch helper / Proton setup.
    return QProcessEnvironment();
}

QProcessEnvironment BedrockInstance::createLaunchEnvironment()
{
    // TODO(Windows/Linux backend steps): see createEnvironment().
    return QProcessEnvironment();
}

QStringList BedrockInstance::getLogFileSearchPaths()
{
    // TODO(Windows/Linux backend steps): the places the Bedrock game actually writes
    // logs to, so the Other Logs page can find them.
    return {};
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
    out << "  install/launch backend: not implemented yet (Windows GDK / legacy UWP pipelines: next update)";
    // TODO(Windows/Linux backend steps): backend-specific details (installed package,
    // pipeline used, Proton prefix, ...).
    return out;
}

void BedrockInstance::updateRuntimeContext()
{
    // NOOP for now.
}
