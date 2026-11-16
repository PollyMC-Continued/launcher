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
#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QStringList>
#include <memory>
#include <optional>

struct archive;
struct archive_entry;
namespace MMCZip {
class ArchiveReader {
   public:
    using ArchivePtr = std::unique_ptr<struct archive, int (*)(struct archive*)>;
    ArchiveReader(QString fileName) : m_archivePath(fileName) {}
    virtual ~ArchiveReader() = default;

    QStringList getFiles();
    QString getZipName();
    bool collectFiles(bool onlyFiles = true);
    bool exists(const QString& filePath) const;
    /// Total uncompressed size of the collected files whose names start with \a prefix.
    /// Only meaningful after a successful collectFiles().
    qint64 totalSizeOf(const QString& prefix = QString()) const;

    class File {
       public:
        File();
        virtual ~File() = default;

        QString filename();
        bool isFile();
        QDateTime dateTime();
        const char* error();
        /// The reason the last writeFile() failed, with the system error included
        /// (e.g. "Write failed: No space left on device"). Empty when it did not fail.
        QString lastError() const { return m_lastError; }

        QByteArray readAll(int* outStatus = nullptr);
        bool skip();
        bool writeFile(archive* out, QString targetFileName = "", bool notBlock = false);
        bool writeFile(archive* out, QString targetFileName, std::optional<QDir> root, bool notBlock = false);

       private:
        int readNextHeader();

       private:
        friend ArchiveReader;
        ArchivePtr m_archive;
        archive_entry* m_entry;
        QString m_lastError;
    };

    std::unique_ptr<File> goToFile(QString filename);
    bool parse(std::function<bool(File*)>);
    bool parse(std::function<bool(File*, bool&)>);

   private:
    QString m_archivePath;
    size_t m_blockSize = 10240;

    QStringList m_fileNames = {};
    QList<qint64> m_fileSizes = {};
};
}  // namespace MMCZip
