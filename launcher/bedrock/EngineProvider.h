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

#include <QString>

#include "tasks/Task.h"

/*
 * The boundary to the game's runtime engine (the Linux side of Bedrock
 * support).
 *
 * Phase 0 decision: the engine is the specialized GDK-Proton/WineGDK build
 * (stock GE-Proton cannot run the game), fetched at runtime and verified
 * against a pinned SHA-256, never bundled - the Weather-OS/GDK-Proton
 * repository carries no license file, so shipping its bytes would have no
 * grant; fetching them at runtime keeps the launcher clean and the decision
 * reversible (the pin names exactly one reviewed set of bytes).
 *
 * The engine is run through umu-launcher (an external GPL-3 process) inside
 * Valve's Steam Linux Runtime, because the GDK networking only works there.
 */
class EngineProvider {
   public:
    virtual ~EngineProvider() = default;

    /* Stable id ("gdk-proton"). */
    virtual QString id() const = 0;

    /* Human-readable name for logs and messages. */
    virtual QString displayName() const = 0;

    /*
     * The engine root directory (the extracted, hash-verified engine), used
     * as PROTONPATH. Empty while the engine is not installed.
     */
    virtual QString engineRoot() const = 0;

    /*
     * The umu-run entry point that launches Windows executables through the
     * engine inside the Steam Linux Runtime. Empty while not installed.
     */
    virtual QString umuRunPath() const = 0;

    /*
     * Directory with the open-source GDK dependency DLLs (libHttpClient.GDK.dll,
     * XCurl.dll) and the CA bundle. Empty while not installed.
     */
    virtual QString gdkDepsDir() const = 0;

    /* True once every engine artifact is in place and hash-verified. */
    virtual bool isInstalled() const = 0;

    /*
     * Downloads, verifies and unpacks the engine artifacts; the returned task
     * succeeds immediately when everything is already in place. This is the
     * "first launch downloads the engine" task - a few hundred MB, all cached
     * and re-used afterwards.
     */
    virtual Task::Ptr ensureInstalled() = 0;
};
