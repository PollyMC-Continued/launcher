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

#include "BaseInstance.h"

/*
 * Bedrock Edition instance.
 *
 * A Bedrock instance is a data profile: its folder holds the chosen game version,
 * its own game data (worlds, resource packs, behavior packs - linked to the
 * installed game on launch, Phase 2 grows the content pages on top of it) and,
 * on Linux, the extracted game files and the per-instance Proton prefix.
 *
 * On Windows the shared installed game (GDK package or legacy UWP
 * registration) is launched with this profile; on Linux the game files are
 * downloaded and decrypted into the instance (Proton pipeline).
 *
 * This class deliberately inherits from BaseInstance, NOT MinecraftInstance: none of
 * the Java machinery (PackProfile components, Java checks, Mojang auth) applies.
 * The instance type string stored in instance.cfg is "Bedrock".
 */
class BedrockInstance : public BaseInstance {
    Q_OBJECT
   public:
    BedrockInstance(SettingsObject* globalSettings, std::unique_ptr<SettingsObject> settings, const QString& rootDir);
    virtual ~BedrockInstance() = default;

    void saveNow() override;
    void loadSpecificSettings() override;

    QString typeName() const override;
    QSet<QString> traits() const override;

    bool canEdit() const override { return true; }
    // TODO(Phase 2): export of Bedrock data profiles (worlds, packs) once content
    // management exists.
    bool canExport() const override { return false; }

    void populateLaunchMenu(QMenu* menu) override;

    ////// Directories and files //////
    QString modsRoot() const override;
    QString instanceConfigFolder() const override;

    /*
     * The Bedrock game version selected for this instance (e.g. "1.21.120.20").
     * Empty until the instance's Version page (or, later, the creation dialog)
     * sets it. Persisted in instance.cfg.
     */
    QString bedrockVersion();
    void setBedrockVersion(const QString& version);

    ////// Launch stuff //////
    QList<Task::Ptr> createUpdateTask() override;
    LaunchTask* createLaunchTask(AuthSessionPtr account, MinecraftTarget::Ptr targetToJoin) override;
    QProcessEnvironment createEnvironment() override;
    QProcessEnvironment createLaunchEnvironment() override;
    QStringList getLogFileSearchPaths() override;
    QString getStatusbarDescription() override;
    QMap<QString, QString> getVariables() override;
    QStringList verboseDescription(AuthSessionPtr session, MinecraftTarget::Ptr targetToJoin) override;

    void updateRuntimeContext() override;
};
