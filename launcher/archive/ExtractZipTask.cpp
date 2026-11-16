// SPDX-License-Identifier: GPL-3.0-only
/*
 *  PollyMC-Continued - Minecraft Launcher
 *  Copyright (c) 2025 Trial97 <alexandru.tripon97@gmail.com>
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
#include "ExtractZipTask.h"
#include <QStorageInfo>
#include <QtConcurrent>
#include "FileSystem.h"
#include "archive/ArchiveReader.h"
#include "archive/ArchiveWriter.h"

namespace MMCZip {

namespace {
// rounded to one decimal, e.g. "1.4 GiB" or "812 MiB"
QString humanSize(qint64 bytes)
{
    constexpr qint64 MiB = 1024 * 1024;
    constexpr qint64 GiB = 1024 * MiB;
    if (bytes >= GiB)
        return ExtractZipTask::tr("%1 GiB").arg(qreal(bytes) / GiB, 0, 'f', 1);
    return ExtractZipTask::tr("%1 MiB").arg(qreal(bytes) / MiB, 0, 'f', 0);
}
}  // namespace

void ExtractZipTask::executeTask()
{
    m_zipFuture = QtConcurrent::run(QThreadPool::globalInstance(), [this]() { return extractZip(); });
    connect(&m_zipWatcher, &QFutureWatcher<ZipResult>::finished, this, &ExtractZipTask::finish);
    m_zipWatcher.setFuture(m_zipFuture);
}

auto ExtractZipTask::extractZip() -> ZipResult
{
    auto target = m_outputDir.absolutePath();
    auto target_top_dir = QUrl::fromLocalFile(target);

    QStringList extracted;

    qDebug() << "Extracting subdir" << m_subdirectory << "from" << m_input.getZipName() << "to" << target;
    if (!m_input.collectFiles()) {
        return ZipResult(tr("Failed to enumerate files in archive"));
    }
    if (m_input.getFiles().isEmpty()) {
        logWarning(tr("Extracting empty archives seems odd..."));
        return ZipResult();
    }

    // Fail before unpacking hundreds of files, not in the middle of it: the archive's
    // uncompressed size (plus a margin for filesystem overhead and the files the launcher
    // writes afterwards) has to fit on the drive the target folder lives on.
    {
        const QStorageInfo storage(target);
        const qint64 needed = m_input.totalSizeOf(m_subdirectory);
        // the bound also guards against archives whose headers lie about their sizes
        if (needed > 0 && needed < (1LL << 62)) {
            const qint64 margin = qMax<qint64>(needed / 10, 64LL * 1024 * 1024);
            if (storage.isValid() && storage.bytesAvailable() >= 0 && needed + margin > storage.bytesAvailable()) {
                return ZipResult(tr("Not enough free space to extract %1: about %2 is needed, but only %3 is free on %4.")
                                     .arg(m_input.getZipName(), humanSize(needed + margin), humanSize(storage.bytesAvailable()),
                                          storage.rootPath()));
            }
        }
    }

    auto extPtr = ArchiveWriter::createDiskWriter();
    auto ext = extPtr.get();

    setStatus("Extracting files...");
    setProgress(0, m_input.getFiles().count());
    ZipResult result;
    auto fileName = m_input.getZipName();
    if (!m_input.parse([this, &result, &target, &target_top_dir, ext, &extracted](ArchiveReader::File* f) {
            if (m_zipFuture.isCanceled())
                return false;
            setProgress(m_progress + 1, m_progressTotal);
            QString file_name = f->filename();
            if (!file_name.startsWith(m_subdirectory)) {
                f->skip();
                return true;
            }

            auto relative_file_name = QDir::fromNativeSeparators(file_name.mid(m_subdirectory.size()));
            auto original_name = relative_file_name;
            setStatus("Unpacking: " + relative_file_name);

            // Fix subdirs/files ending with a / getting transformed into absolute paths
            if (relative_file_name.startsWith('/'))
                relative_file_name = relative_file_name.mid(1);

            // Fix weird "folders with a single file get squashed" thing
            QString sub_path;
            if (relative_file_name.contains('/') && !relative_file_name.endsWith('/')) {
                sub_path = relative_file_name.section('/', 0, -2) + '/';
                FS::ensureFolderPathExists(FS::PathCombine(target, sub_path));

                relative_file_name = relative_file_name.split('/').last();
            }

            QString target_file_path;
            if (relative_file_name.isEmpty()) {
                target_file_path = target + '/';
            } else {
                target_file_path = FS::PathCombine(target_top_dir.toLocalFile(), sub_path, relative_file_name);
                if (relative_file_name.endsWith('/') && !target_file_path.endsWith('/'))
                    target_file_path += '/';
            }

            if (!target_top_dir.isParentOf(QUrl::fromLocalFile(target_file_path))) {
                result = ZipResult(tr("Extracting %1 was cancelled, because it was effectively outside of the target path %2")
                                       .arg(relative_file_name, target));
                return false;
            }

            if (!f->writeFile(ext, target_file_path, target)) {
                // the half-written file has to go too, so no partial file is left behind
                extracted.append(target_file_path);
                const QString reason = f->lastError();
                result = ZipResult(tr("Failed to extract file %1 to %2%3")
                                       .arg(original_name, target_file_path,
                                            reason.isEmpty() ? QString() : QStringLiteral(": %1").arg(reason)));
                return false;
            }
            extracted.append(target_file_path);

            qDebug() << "Extracted file" << relative_file_name << "to" << target_file_path;
            return true;
        })) {
        FS::removeFiles(extracted);
        return result.has_value() ? result : ZipResult(tr("Failed to parse file %1").arg(fileName));
    }
    return ZipResult();
}

void ExtractZipTask::finish()
{
    if (m_zipFuture.isCanceled()) {
        emitAborted();
    } else if (auto result = m_zipFuture.result(); result.has_value()) {
        emitFailed(result.value());
    } else {
        emitSucceeded();
    }
}

bool ExtractZipTask::abort()
{
    if (m_zipFuture.isRunning()) {
        m_zipFuture.cancel();
        // NOTE: Here we don't do `emitAborted()` because it will be done when `m_build_zip_future` actually cancels, which may not occur
        // immediately.
        return true;
    }
    return false;
}

}  // namespace MMCZip
