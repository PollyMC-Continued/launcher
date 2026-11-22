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

class BedrockInstance;

/*
 * Shared locations for Bedrock runtime artifacts.
 *
 * Everything the install/launch pipelines download lives under the launcher's
 * data directory, never inside the source tree and never per-instance except
 * the per-instance game data itself:
 *
 *   <dataRoot>/bedrock/
 *     packages/<versionId>/       shared game package cache (.msixvc / .appx)
 *     engines/<dir>               GDK-Proton engine (Linux)
 *     umu/                         umu-launcher zipapp (Linux)
 *     gdkdeps/                     OSS GDK dependency DLLs + CA bundle (Linux)
 *     xodus/                       licensing client binary (Linux)
 *     xodus-home/                  isolated HOME for the licensing client
 *     openssl-xcurl/               online-login payload for the engine (Linux)
 *     helper/                      Windows launch helper (gamelaunchhelper.dll)
 *
 * Definitions live in BedrockInstallTask.cpp.
 */
namespace BedrockPaths {
/* <dataRoot>/bedrock */
QString bedrockDataDir();
QString packagesDir();
/* <packagesDir>/<versionId> */
QString packageDir(const QString& versionId);
QString enginesDir();
QString umuDir();
QString gdkDepsDir();
QString xodusDir();
QString xodusHomeDir();
QString opensslXcurlDir();
QString windowsHelperDir();

/* instanceRoot()/game - the instance's own game data (extracted game files). */
QString instanceGameDir(BedrockInstance* instance);
/* instanceRoot()/gamedata - the instance's own game data profile (worlds, packs, settings) on Windows. */
QString instanceGameDataDir(BedrockInstance* instance);
/* instanceRoot()/proton-prefix - the per-instance Wine prefix (Linux). */
QString instancePrefixDir(BedrockInstance* instance);
}  // namespace BedrockPaths

/*
 * Runtime artifacts the Linux/Windows backends fetch on demand, with the
 * SHA-256 pins the Phase 0 verification locked in (re-verified while
 * implementing this step).
 *
 * Rule: nothing of this is bundled with the launcher, nothing is re-hosted:
 * every artifact is fetched from its upstream release at runtime and verified
 * against the pinned hash before it is used. A pinned artifact whose download
 * does not match its hash is refused rather than used (the BedrockOnLinux
 * project's own policy, which these pins come from).
 *
 * Sources of the pins (fetched live):
 *  - Wyze3306/BedrockOnLinux bol/config.py (MIT): umu zipapp + umu_run hashes,
 *    WineGDK engine archive hash and revision, Xodus build hash and revision,
 *    OpenSSL XCurl set hash and revision, GDK dependency DLL hashes, MSA
 *    constants, product ids.
 *  - BedrockLauncher release page (GPL-3): helper archive URL and SHA-256.
 */
namespace BedrockArtifacts {

struct Artifact {
    /* Human-readable label for logs and the settings page. */
    QString name;
    /* Direct download URL; empty when resolved through the GitHub releases API. */
    QString url;
    /* "owner/repo" for GitHub-releases resolution when url is empty. */
    QString repo;
    /* Asset-name substring to find in the release listing when url is empty. */
    QString assetPattern;
    /* File name of the downloaded artifact (cache side). */
    QString fileName;
    /* Pinned SHA-256 of the downloaded artifact; empty = unversioned (CA bundle). */
    QString sha256;
    /* Upstream license, shown in the log when the artifact is installed. */
    QString license;
};

/* umu-launcher zipapp (GPL-3, Open-Wine-Components/umu-launcher 1.4.3). */
inline Artifact umuZipapp()
{
    Artifact artifact{};
    artifact.name = QStringLiteral("umu-launcher zipapp");
    artifact.url = QStringLiteral("https://github.com/Open-Wine-Components/umu-launcher/releases/download/1.4.3/umu-launcher-1.4.3-zipapp.tar");
    artifact.fileName = QStringLiteral("umu-launcher-1.4.3-zipapp.tar");
    artifact.sha256 = QStringLiteral("3f8fdc033f547afdb3408ea48ad07194769405148dcfa2b2f945b7fb368a33bb");
    artifact.license = QStringLiteral("GPL-3");
    return artifact;
}

/*
 * Layout of the pinned 1.4.3 zipapp (verified against the pinned archive): the
 * tar unpacks one top-level "umu/" directory that holds the entry point
 * "umu-run" (the Python zipapp itself) and a "umu_run.py" compatibility
 * symlink pointing at it. Upstream renamed the underscore entry point of the
 * older zipapps to the hyphenated one, so there is no "umu_run" file anywhere
 * in a 1.4.3 archive - pinning that name made every first launch abort with
 * 'the expected file "umu_run" was not found in it' after an otherwise
 * successful, hash-verified download.
 */
inline QString umuRunDirName()
{
    return QStringLiteral("umu");
}

inline QString umuRunName()
{
    return QStringLiteral("umu-run");
}

/* SHA-256 of the umu-run entry point above, re-verified against the pinned archive. */
inline QString umuRunSha256()
{
    return QStringLiteral("577181dbff2eccdaa78b411c0fd1aa7fde574028449c3e0e99f508536a76870e");
}

/*
 * The WineGDK engine (GDK-Proton). The prebuilt engine is published on the
 * BedrockOnLinux releases; the pin below names the reviewed build revision.
 * The Weather-OS/GDK-Proton sources carry no license file, which is exactly
 * why the engine is fetched at runtime and never bundled - see TESTING.md.
 */
inline Artifact winegdkEngine()
{
    Artifact artifact{};
    artifact.name = QStringLiteral("GDK-Proton engine (WineGDK)");
    artifact.repo = QStringLiteral("Wyze3306/BedrockOnLinux");
    artifact.assetPattern = QStringLiteral("wow64-archs-native22");
    artifact.fileName = QStringLiteral("GDK-Proton-xuser-wow64-archs-native22.tar");
    artifact.sha256 = QStringLiteral("46927aa8d180c7151a6240c5e2196136cd515041292dd848218ea809284cb3ed");
    artifact.license = QStringLiteral("Weather-OS/GDK-Proton build (no upstream license file; fetched at runtime, never bundled)");
    return artifact;
}

inline QString winegdkEngineDirName()
{
    return QStringLiteral("GDK-Proton-xuser-wow64-archs-native22");
}

/*
 * Open-source replacements for the GDK network DLLs
 * (minecraft-linux/mcpelauncher-gdk-dependencies). The DLLs run inside the
 * game, so only the pinned bytes are installed.
 */
inline Artifact gdkDependency(const QString& fileName)
{
    Artifact artifact{};
    artifact.name = fileName;
    artifact.url = QStringLiteral("https://github.com/minecraft-linux/mcpelauncher-gdk-dependencies/releases/download/v0.0.0/") + fileName;
    artifact.fileName = fileName;
    if (fileName == QLatin1String("libHttpClient.GDK.dll")) {
        artifact.sha256 = QStringLiteral("718f0c6842c8e35d7cac5c18ec025404c83be03ec035f0d3904e70d6508b9e24");
    } else if (fileName == QLatin1String("XCurl.dll")) {
        artifact.sha256 = QStringLiteral("28443194e9860733f24294219bcb45226677636f9721d4bf145094d977f7161f");
    }
    artifact.license = QStringLiteral("minecraft-linux/mcpelauncher-gdk-dependencies");
    return artifact;
}

/* The CA bundle the GDK network stack trusts (no pin: it is a living file). */
inline Artifact caBundle()
{
    Artifact artifact{};
    artifact.name = QStringLiteral("CA certificate bundle");
    artifact.url = QStringLiteral("https://curl.se/ca/cacert.pem");
    artifact.fileName = QStringLiteral("ca-bundle.crt");
    artifact.license = QStringLiteral("curl.se CA bundle");
    return artifact;
}

/*
 * The OpenSSL XCurl set: the online-login payload the engine needs, pinned by
 * the BedrockOnLinux project ("Integrity pin for the complete online-login
 * payload").
 */
inline Artifact opensslXcurlSet()
{
    Artifact artifact{};
    artifact.name = QStringLiteral("OpenSSL XCurl online-login payload");
    artifact.repo = QStringLiteral("Wyze3306/BedrockOnLinux");
    artifact.assetPattern = QStringLiteral("504bb166e4e7");
    artifact.fileName = QStringLiteral("openssl-set-504bb166e4e7.tar");
    artifact.sha256 = QStringLiteral("504bb166e4e737ad81c3ac8e7a917740b28478f69acd89e538c3bf921c29523f");
    artifact.license = QStringLiteral("Wyze3306/BedrockOnLinux (MIT)");
    return artifact;
}

/*
 * Xodus, the external GPL-3 licensing client (xodus-gaming/xodus at commit
 * 64d39eb87a56, patched build p11 as published by BedrockOnLinux). It is run
 * as a separate process and given its own HOME so its token keyring persists;
 * nothing of it is linked into the launcher. A user-supplied binary (instance
 * setting BedrockXodusPath) always wins over the managed copy.
 */
inline Artifact xodusClient()
{
    Artifact artifact{};
    artifact.name = QStringLiteral("Xodus licensing client");
    artifact.repo = QStringLiteral("Wyze3306/BedrockOnLinux");
    artifact.assetPattern = QStringLiteral("64d39eb87a56-p11");
    artifact.fileName = QStringLiteral("xodus-64d39eb87a56-p11.tar");
    artifact.sha256 = QStringLiteral("adb23afbcf6315b4278ec01e06ee9d512a3ea705d0ed182c26d9a1a5952d853a");
    artifact.license = QStringLiteral("GPL-3 (xodus-gaming/xodus)");
    return artifact;
}

inline QString xodusBinaryName()
{
    return QStringLiteral("xodus-cli");
}

/*
 * BedrockLauncher's gamelaunchhelper.dll (GPL-3): the optional Windows helper
 * that pins the exact installed GDK version instead of the Windows bootstrapper
 * that auto-updates the game. Optional: without it the installed version is
 * activated as-is.
 */
inline Artifact windowsLaunchHelper()
{
    Artifact artifact{};
    artifact.name = QStringLiteral("gamelaunchhelper.dll");
    artifact.url = QStringLiteral("https://github.com/BedrockLauncher/BedrockLauncher/releases/download/2026.10.19.270/BedrockLauncher-2026.10.19.270.zip");
    artifact.fileName = QStringLiteral("BedrockLauncher-2026.10.19.270.zip");
    artifact.sha256 = QStringLiteral("d23a5e778f1250afd258da4e03d1617fe6e7b435133101f9bfd522e06b666097");
    artifact.license = QStringLiteral("GPL-3 (BedrockLauncher)");
    return artifact;
}

/* Path of the helper DLL inside the BedrockLauncher release zip. */
inline QString windowsLaunchHelperInnerPath()
{
    return QStringLiteral("native/gamelaunchhelper/gamelaunchhelper.dll");
}

/* Game sign-in under WineGDK: the registry key the engine reads the refresh token from. */
inline QString winegdkTokenRegistryKey()
{
    return QStringLiteral("Software\\Wine\\WineGDK");
}

inline QString winegdkTokenRegistryValue()
{
    /*
     * BedrockOnLinux documents the key, not the value name; this one matches
     * their flow and is flagged for verification in TESTING.md.
     */
    return QStringLiteral("RefreshToken");
}

}  // namespace BedrockArtifacts
