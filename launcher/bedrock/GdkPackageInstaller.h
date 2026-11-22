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

/*
 * The Windows side of the install/launch pipelines, behind the same build on
 * every platform:
 *
 *  - current GDK packages (.msixvc): the file is handed to
 *    PackageManager.AddPackageAsync so Windows Gaming Services performs the
 *    entitled install (the user must own the game - nothing here bypasses
 *    that), and the installed package is activated through its
 *    AppUserModelID, the BedrockLauncher-proven flow;
 *  - legacy UWP packages (.appx): the extracted, signature-stripped folder is
 *    registered in developer mode and activated.
 *
 * The WinRT/COM work runs on a worker thread (the GUI thread is an STA); the
 * functions block until the deployment operation completes and return the
 * service's own error text on failure.
 *
 * On non-Windows platforms every function fails with a clear reason, so the
 * shared code paths stay compilable everywhere.
 */
namespace GdkPackageInstaller {

/* True when this platform can install/activate Windows game packages. */
bool isSupported();

/*
 * Installs a package file (.msixvc of the current GDK era, or an .appx of the
 * legacy UWP era) for the current user. Returns false with the deployment
 * service's own error text in *error on failure.
 */
bool installPackageFile(const QString& packageFilePath, QString* error);

/*
 * Registers an extracted package folder in developer mode: manifestPath points
 * at its AppxManifest.xml. The first attempt registers the manifest as-is;
 * when the deployment service rejects it over missing package dependencies,
 * the manifest is rewritten without its <PackageDependency> entries and the
 * registration is retried once (the original manifest is kept as a backup
 * file). Developer mode must be enabled on the machine.
 */
bool registerExtractedPackage(const QString& manifestPath, QString* error);

/*
 * The full name of the installed Minecraft package
 * ("Microsoft.MinecraftUWP_1.26.4201.0_x64__8wekyb3d8bbwe"), or the empty
 * string when no Minecraft package is installed for the current user.
 */
QString installedMinecraftPackageFullName();

/* The install directory of the named package; empty when it is not installed. */
QString installedPackageLocation(const QString& fullName);

/*
 * The AppUserModelID of the named package's application
 * ("<family name>!<application id>"), read from the installed manifest.
 */
QString aumidForPackage(const QString& fullName);

/* Activates an installed application through its AppUserModelID; the PID, or -1 on failure. */
qint64 activateAumid(const QString& aumid, const QString& arguments, QString* error);

/* Opens a URL through its protocol handler (the "minecraft:" protocol of the registered game). */
bool activateProtocol(const QString& protocolUrl, QString* error);

}  // namespace GdkPackageInstaller
