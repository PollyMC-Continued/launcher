// SPDX-License-Identifier: GPL-3.0-only
/*
 *  PollyMC-Continued - Minecraft Launcher
 *
 *  Names of the files shared between the launcher and the updater.
 *
 *  Older releases used "prism_launcher"/"prismlauncher" in these names. New files are always written
 *  with the PollyMC names; the legacy names are still read so that updating from (or interrupted by)
 *  an older release keeps working.
 */

#pragma once

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>

namespace UpdaterNames {

struct Name {
    const char* current;
    const char* legacy;
};

inline constexpr Name SettingsFile{ "pollymc_update.cfg", "prismlauncher_update.cfg" };
inline constexpr Name LogFile{ "pollymc_update.log", "prism_launcher_update.log" };
inline constexpr Name LockFile{ ".pollymc_update.lock", ".prism_launcher_update.lock" };
inline constexpr Name FailMarker{ ".pollymc_update.fail", ".prism_launcher_update.fail" };
inline constexpr Name SuccessMarker{ ".pollymc_update.success", ".prism_launcher_update.success" };
inline constexpr Name ChangelogFile{ ".pollymc_update.changelog", ".prism_launcher_update.changelog" };
inline constexpr Name BackupPathFile{ ".pollymc_update_backup_path.txt", ".prism_launcher_update_backup_path.txt" };
inline constexpr Name UnpackMarker{ ".pollymc_updater_unpack.marker", ".prism_launcher_updater_unpack.marker" };
inline constexpr Name ReleaseTempDir{ "pollymc_update_release", "prism_launcher_update_release" };

/// Path using the current (PollyMC) name. Use this for anything that is written.
inline QString currentPath(const QString& dir, const Name& name)
{
    return QDir(dir).filePath(QString::fromLatin1(name.current));
}

/// Path using the legacy (Prism) name.
inline QString legacyPath(const QString& dir, const Name& name)
{
    return QDir(dir).filePath(QString::fromLatin1(name.legacy));
}

/// Path to read from: the current file if it exists, else the legacy one if it exists,
/// else the current path (so a "does it exist?" check on the result is correct).
inline QString resolve(const QString& dir, const Name& name)
{
    const QString cur = currentPath(dir, name);
    if (QFileInfo::exists(cur))
        return cur;
    const QString old = legacyPath(dir, name);
    if (QFileInfo::exists(old))
        return old;
    return cur;
}

/// Copies the legacy file to the current name once (the legacy file is left in place). Returns the current path.
inline QString migrateFile(const QString& dir, const Name& name)
{
    const QString cur = currentPath(dir, name);
    const QString old = legacyPath(dir, name);
    if (!QFileInfo::exists(cur) && QFileInfo::exists(old))
        QFile::copy(old, cur);
    return cur;
}

/// Removes both the current and the legacy file (or directory) if present.
inline void removeAll(const QString& dir, const Name& name)
{
    for (const QString& path : { currentPath(dir, name), legacyPath(dir, name) }) {
        QFileInfo info(path);
        if (info.isDir())
            QDir(path).removeRecursively();
        else if (info.exists())
            QFile::remove(path);
    }
}

}  // namespace UpdaterNames
