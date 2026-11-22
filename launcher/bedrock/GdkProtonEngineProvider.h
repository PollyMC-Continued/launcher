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

#include <QProcessEnvironment>
#include <QObject>

#include "BedrockArtifacts.h"
#include "EngineProvider.h"

/*
 * The GDK-Proton implementation of EngineProvider (Linux).
 *
 * The engine is the verified hybrid stack of BedrockOnLinux: a GE-Proton
 * base overlaid with the WineGDK build from Weather-OS/GDK-Proton (pinned),
 * plus the open-source GDK dependency DLLs, the CA bundle and the online-login
 * payload. Everything is fetched at runtime, verified against the pinned
 * SHA-256 values and unpacked under the launcher's data directory; nothing is
 * bundled with the launcher (the Weather-OS repository carries no license
 * file - see TESTING.md).
 *
 * The engine runs through umu-launcher inside Valve's Steam Linux Runtime,
 * because the GDK networking (LAN, server join) only works in that
 * environment; stock GE-Proton cannot run the game.
 */
class GdkProtonEngineProvider : public QObject, public EngineProvider {
    Q_OBJECT
   public:
    /* The application-wide engine provider (parented to the application object). */
    static GdkProtonEngineProvider* get();

    ////// EngineProvider //////

    QString id() const override;
    QString displayName() const override;
    QString engineRoot() const override;
    QString umuRunPath() const override;
    QString gdkDepsDir() const override;
    bool isInstalled() const override;
    Task::Ptr ensureInstalled() override;

    /*
     * The environment variables the umu-run invocation needs for a specific
     * prefix: PROTONPATH to the managed engine, WINEPREFIX to the prefix,
     * PROTON_VERB, GAMEID and STORE. On the record of the verified recipe -
     * see TESTING.md for the parts flagged unverified.
     */
    static QProcessEnvironment launchEnvironment(const QString& engineRoot,
                                                  const QString& winePrefix,
                                                  const QString& xodusHome,
                                                  const QString& gameDir);

   private:
    explicit GdkProtonEngineProvider(QObject* parent = nullptr);

    Task::Ptr artifactTask(const BedrockArtifacts::Artifact& artifact,
                            const QString& destinationDir,
                            bool extract,
                            const QString& expectedInnerFile,
                            bool executable);
};
