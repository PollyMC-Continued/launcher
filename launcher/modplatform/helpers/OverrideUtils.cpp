#include "OverrideUtils.h"

#include <QDir>
#include <QDirIterator>

#include "FileSystem.h"

namespace Override {

void createOverrides(const QString& name, const QString& parent_folder, const QString& override_path)
{
    QString file_path(FS::PathCombine(parent_folder, name + ".txt"));
    if (QFile::exists(file_path))
        FS::deletePath(file_path);

    FS::ensureFilePathExists(file_path);

    QFile file(file_path);
    if (!file.open(QFile::WriteOnly)) {
        qWarning() << "Failed to open file" << file.fileName() << "for writing:" << file.errorString();
        return;
    }

    QDirIterator override_iterator(override_path, QDirIterator::Subdirectories);
    // Everything below the override folder, as a path relative to it. Note: the old
    // `split(name)` trick broke on any file whose path contained the folder name again
    // (e.g. "overrides/config/overrides.cfg"), which then never got cleaned up on updates.
    const QString base = override_path + QLatin1Char('/');
    while (override_iterator.hasNext()) {
        auto override_file_path = override_iterator.next();
        QFileInfo info(override_file_path);
        if (info.isFile()) {
            if (override_file_path.startsWith(base))
                override_file_path = override_file_path.mid(base.size());
            else
                override_file_path = QDir(base).relativeFilePath(override_file_path);

            file.write(override_file_path.toUtf8());
            file.write("\n");
        }
    }

    file.close();
}

QStringList readOverrides(const QString& name, const QString& parent_folder)
{
    QString file_path(FS::PathCombine(parent_folder, name + ".txt"));

    QFile file(file_path);
    if (!file.exists())
        return {};

    QStringList previous_overrides;

    if (!file.open(QFile::ReadOnly)) {
        qWarning() << "Failed to open file" << file.fileName() << "for reading:" << file.errorString();
        return previous_overrides;
    }

    QString entry;
    do {
        entry = file.readLine();
        previous_overrides.append(entry.trimmed());
    } while (!entry.isEmpty());

    file.close();

    return previous_overrides;
}

}  // namespace Override
