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
 */

#pragma once

#include <QString>
#include <QStringList>
#include <QUrl>

/// Software OpenGL (Mesa llvmpipe) for PCs whose graphics hardware or driver cannot run current Minecraft versions.
/// Windows only. It is slow, but it works on almost any computer.
namespace SoftwareRenderer {

/// Where the Mesa DLLs live (inside the launcher's data folder).
QString folder();
/// Full path of Mesa's opengl32.dll, or an empty string when it is not installed.
QString openGlLibrary();
bool isInstalled();

/// Whether this build knows where to download Mesa from (and how to verify it).
bool canDownload();
QUrl downloadUrl();
QString downloadSha256();
QString archivePath();
/// Pulls the 64-bit Mesa DLLs out of the downloaded archive into folder().
bool extract(const QString& archive, QString& error);

/// True when the settings ask for it, Mesa is installed and the Java in use is 64-bit (or unknown).
bool active(bool enabled, const QString& javaArchitecture);

/// The graphics adapters Windows reports.
QStringList detectedGpus();
/// True when every adapter is either Windows' built-in "Basic Render Driver" or a very old Intel chip.
/// Meant for a friendly warning only, so it errs on the side of saying nothing.
bool gpuLooksUnsupported();

}  // namespace SoftwareRenderer
