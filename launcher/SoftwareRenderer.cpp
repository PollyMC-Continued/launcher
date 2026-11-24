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

#include "SoftwareRenderer.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>

#include "Application.h"
#include "FileSystem.h"
#include "HardwareInfo.h"
#include "archive/ArchiveReader.h"

namespace {
// Mesa3D for Windows, built by https://github.com/pal1000/mesa-dist-win (MIT licensed).
constexpr const char* MesaVersion = "26.2.4";
// SHA-256 of the archive below. Downloads are refused while this is empty, so nothing unverified is ever loaded.
// Fill it in with: curl -L -o mesa.7z <URL> && sha256sum mesa.7z
constexpr const char* MesaSha256 = "";

const QStringList s_wantedFiles = { "opengl32.dll", "libgallium_wgl.dll", "libglapi.dll" };
}  // namespace

namespace SoftwareRenderer {

QString folder()
{
    return FS::PathCombine(APPLICATION->dataRoot(), "mesa");
}

QString openGlLibrary()
{
    const QString path = FS::PathCombine(folder(), "opengl32.dll");
    return QFileInfo::exists(path) ? QDir::toNativeSeparators(path) : QString();
}

bool isInstalled()
{
    return !openGlLibrary().isEmpty();
}

QUrl downloadUrl()
{
    return QUrl(QString("https://github.com/pal1000/mesa-dist-win/releases/download/%1/mesa3d-%1-release-msvc.7z").arg(MesaVersion));
}

QString downloadSha256()
{
    return QString::fromLatin1(MesaSha256).trimmed();
}

bool canDownload()
{
    return downloadSha256().size() == 64;
}

QString archivePath()
{
    return FS::PathCombine(folder(), "mesa.7z.part");
}

bool extract(const QString& archive, QString& error)
{
    MMCZip::ArchiveReader reader(archive);
    if (!reader.collectFiles()) {
        error = QObject::tr("The downloaded Mesa archive could not be read.");
        return false;
    }
    if (!QDir().mkpath(folder())) {
        error = QObject::tr("Could not create the folder %1.").arg(folder());
        return false;
    }

    const QStringList names = reader.getFiles();
    for (const QString& want : s_wantedFiles) {
        QString entry;
        for (const QString& name : names) {
            const QString normalized = name.toLower().replace('\\', '/');
            if (normalized == "x64/" + want || normalized.endsWith("/x64/" + want)) {
                entry = name;
                break;
            }
        }
        if (entry.isEmpty()) {
            if (want == "opengl32.dll") {
                error = QObject::tr("The Mesa archive does not contain a 64-bit opengl32.dll.");
                return false;
            }
            continue;  // newer/older Mesa layouts do not ship every helper library
        }

        auto file = reader.goToFile(entry);
        if (!file) {
            error = QObject::tr("Could not open %1 inside the Mesa archive.").arg(entry);
            return false;
        }
        const QByteArray data = file->readAll();
        if (data.isEmpty()) {
            error = QObject::tr("%1 inside the Mesa archive is empty or damaged.").arg(entry);
            return false;
        }

        QSaveFile out(FS::PathCombine(folder(), want));
        if (!out.open(QIODevice::WriteOnly) || out.write(data) != data.size() || !out.commit()) {
            error = QObject::tr("Could not write %1: %2").arg(want, out.errorString());
            return false;
        }
    }
    return true;
}

bool active(bool enabled, const QString& javaArchitecture)
{
#ifdef Q_OS_WIN
    if (!enabled || !isInstalled())
        return false;
    // The DLLs we ship are 64-bit. A 32-bit Java cannot load them.
    const QString arch = javaArchitecture.toLower();
    return arch.isEmpty() || arch.contains("64") || arch.contains("aarch");
#else
    Q_UNUSED(enabled)
    Q_UNUSED(javaArchitecture)
    return false;
#endif
}

QStringList detectedGpus()
{
    QStringList out;
    for (const QString& line : HardwareInfo::gpuInfo()) {
        if (line.startsWith("GPU: "))
            out << line.mid(5).trimmed();
    }
    return out;
}

bool gpuLooksUnsupported()
{
    const QStringList gpus = detectedGpus();
    if (gpus.isEmpty())
        return false;

    static const QRegularExpression oldIntel(R"(^intel\(r\) (hd )?graphics( (2000|3000))?$|\bgma\b)");
    for (const QString& gpu : gpus) {
        const QString name = gpu.toLower().trimmed();
        const bool basic = name.contains("basic render") || name.contains("basic display");
        const bool old = oldIntel.match(name).hasMatch();
        if (!basic && !old)
            return false;  // at least one real, reasonably modern adapter: say nothing
    }
    return true;
}

}  // namespace SoftwareRenderer
