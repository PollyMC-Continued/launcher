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

#include "InstanceTemplateTask.h"

#include <QDateTime>
#include <QDebug>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QtConcurrentRun>

#include <memory>

#include "Application.h"
#include "FileSystem.h"
#include "NullInstance.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "settings/INISettingsObject.h"
#include "icons/IconList.h"   

namespace {
/* Folders that never make it into a template. They live under the game root
 * (minecraft/ or .minecraft/), but the filter also matches them directly in the
 * instance root, for safety. */
const QStringList alwaysExcludedFolders = { "saves", "logs", "crash-reports", "screenshots", "backups" };

/* Folders and files that are only copied when the corresponding entry is in the
 * includeFolders whitelist. */
const QStringList modsFolders = { "mods", "coremods", "nilmods" };
const QStringList configsFolders = { "config" };
const QStringList configsFiles = { "options.txt" };
const QStringList resourcePacksFolders = { "resourcepacks", "texturepacks", "server-resource-packs" };
const QStringList shaderPacksFolders = { "shaderpacks" };
const QStringList serversFiles = { "servers.dat", "servers.dat_old" };

/* Matches "<prefix>minecraft/" and ".minecraft/" (see InstanceCopyPrefs::getSelectedFiltersAsRegex). */
const QString MC_ROOT = "[.]?minecraft/";

QString escapedAlternatives(const QStringList& entries)
{
    QStringList escaped;
    escaped.reserve(entries.size());
    for (const QString& entry : entries)
        escaped += QRegularExpression::escape(entry);
    return escaped.join('|');
}

/* Folders are matched with a trailing separator so that "mods" does not swallow
 * files such as "mods-backup.dat"; files are matched at the end of the relative path. */
QString folderPatterns(const QStringList& folders)
{
    return "(" + escapedAlternatives(folders) + ")/";
}

QString filePatterns(const QStringList& files)
{
    return "(" + escapedAlternatives(files) + ")$";
}

QString minecraftVersionOf(BaseInstance* source)
{
    auto mcInstance = dynamic_cast<MinecraftInstance*>(source);
    if (!mcInstance)
        return {};
    return mcInstance->getPackProfile()->getComponentVersion("net.minecraft");
}
}  // namespace

SaveTemplateTask::SaveTemplateTask(BaseInstance* source,
                                   QString templateName,
                                   QStringList includeFolders,
                                   QString iconKey,
                                   bool overwrite)
    : m_source(source), m_templateName(std::move(templateName)), m_includeFolders(std::move(includeFolders)), m_iconKey(std::move(iconKey)),
      m_overwrite(overwrite)
{}

QString SaveTemplateTask::templatesRoot()
{
    return FS::PathCombine(APPLICATION->dataRoot(), "templates");
}

QString SaveTemplateTask::sanitizeTemplateDirName(const QString& name)
{
    QString sanitized = name.simplified();
    sanitized.remove(QRegularExpression("[\\\\/:*?\"<>|]"));
    while (!sanitized.isEmpty() && sanitized.startsWith('.'))
        sanitized.remove(0, 1);
    return sanitized;
}

QString SaveTemplateTask::buildExcludeRegex() const
{
    QStringList folders = alwaysExcludedFolders;
    QStringList files;

    if (!m_includeFolders.contains("mods"))
        folders += modsFolders;

    if (!m_includeFolders.contains("configs")) {
        folders += configsFolders;
        files += configsFiles;
    }

    if (!m_includeFolders.contains("resourcepacks"))
        folders += resourcePacksFolders;

    if (!m_includeFolders.contains("shaderpacks"))
        folders += shaderPacksFolders;

    if (!m_includeFolders.contains("servers"))
        files += serversFiles;

    QStringList patterns;
    if (!folders.isEmpty())
        patterns += "^(" + MC_ROOT + ")?" + folderPatterns(folders);
    if (!files.isEmpty())
        patterns += "^(" + MC_ROOT + ")?" + filePatterns(files);

    return patterns.join('|');
}

void SaveTemplateTask::executeTask()
{
    setAbortable(true);

    const QString safeName = sanitizeTemplateDirName(m_templateName);
    if (safeName.isEmpty()) {
        emitFailed(tr("The template name is empty after removing invalid characters."));
        return;
    }

    const QString root = templatesRoot();
    if (!FS::ensureFolderPathExists(root)) {
        emitFailed(tr("Could not create the templates folder: %1").arg(root));
        return;
    }

    m_templateDir = FS::PathCombine(root, safeName);

    if (QFileInfo::exists(m_templateDir)) {
        if (!m_overwrite) {
            emitFailed(tr("A template named %1 already exists.").arg(m_templateName));
            return;
        }

        setStatus(tr("Removing existing template %1").arg(m_templateName));
        if (!FS::deletePath(m_templateDir)) {
            emitFailed(tr("Could not remove the existing template: %1").arg(m_templateDir));
            return;
        }
    }

    const QString filters = buildExcludeRegex();
    qDebug() << "TemplateFilters:" << filters;
    if (!filters.isEmpty()) {
        QRegularExpression regexp(filters, QRegularExpression::CaseInsensitiveOption);
        m_matcher = Filters::regexp(regexp);
    }

    setStatus(tr("Creating template %1 from instance %2").arg(safeName, m_source->name()));

    m_copyFuture = QtConcurrent::run(QThreadPool::globalInstance(), [this] {
        FS::copy folderCopy(m_source->instanceRoot(), m_templateDir);
        folderCopy.matcher(m_matcher);

        folderCopy(true);
        setProgress(0, folderCopy.totalCopied());
        connect(&folderCopy, &FS::copy::fileCopied, [this]() { setProgress(m_progress + 1, m_progressTotal); });
        return folderCopy();
    });
    connect(&m_copyFutureWatcher, &QFutureWatcher<bool>::finished, this, &SaveTemplateTask::copyFinished);
    connect(&m_copyFutureWatcher, &QFutureWatcher<bool>::canceled, this, &SaveTemplateTask::copyAborted);
    m_copyFutureWatcher.setFuture(m_copyFuture);
}

void SaveTemplateTask::copyFinished()
{
    if (!m_copyFuture.result()) {
        emitFailed(tr("Template folder copy failed."));
        return;
    }

    QJsonObject manifest;
    manifest["name"] = m_templateName;
    manifest["created"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    manifest["sourceInstance"] = m_source->name();
    manifest["minecraftVersion"] = minecraftVersionOf(m_source);
    manifest["iconKey"] = m_iconKey;

    try {
        FS::write(FS::PathCombine(m_templateDir, "template.json"), QJsonDocument(manifest).toJson());
    } catch (const FS::FileSystemException& e) {
        qCritical() << "Failed to write template manifest:" << e.cause();
        emitFailed(tr("Could not write the template manifest."));
        return;
    }

    APPLICATION->icons()->saveIcon(m_iconKey, FS::PathCombine(m_templateDir, "icon.png"), "PNG");

    emitSucceeded();
}

void SaveTemplateTask::copyAborted()
{
    emitFailed(tr("Template folder copy has been aborted."));
}

bool SaveTemplateTask::abort()
{
    if (m_copyFutureWatcher.isRunning()) {
        m_copyFutureWatcher.cancel();
        // NOTE: Here we don't do `emitAborted()` because it will be done when `m_copyFutureWatcher` actually cancels, which may not occur
        // immediately.
        return true;
    }
    return false;
}

InstantiateTemplateTask::InstantiateTemplateTask(QString templateDir, QString newInstanceName, QString group, QString iconKey)
    : m_templateDir(std::move(templateDir))
{
    setName(newInstanceName);
    setGroup(group);
    setIcon(iconKey);

    // The manifest and the icon belong to the template itself, not to the instances created from it.
    QRegularExpression regexp("^(template\\.json|icon\\.png)$");
    m_matcher = Filters::regexp(regexp);
}

void InstantiateTemplateTask::executeTask()
{
    setAbortable(true);

    if (!QFileInfo::exists(m_templateDir)) {
        emitFailed(tr("The template folder %1 does not exist.").arg(m_templateDir));
        return;
    }

    setStatus(tr("Creating instance %1 from template").arg(name()));

    m_copyFuture = QtConcurrent::run(QThreadPool::globalInstance(), [this] {
        FS::copy folderCopy(m_templateDir, m_stagingPath);
        folderCopy.matcher(m_matcher);

        folderCopy(true);
        setProgress(0, folderCopy.totalCopied());
        connect(&folderCopy, &FS::copy::fileCopied, [this]() { setProgress(m_progress + 1, m_progressTotal); });
        return folderCopy();
    });
    connect(&m_copyFutureWatcher, &QFutureWatcher<bool>::finished, this, &InstantiateTemplateTask::copyFinished);
    connect(&m_copyFutureWatcher, &QFutureWatcher<bool>::canceled, this, &InstantiateTemplateTask::copyAborted);
    m_copyFutureWatcher.setFuture(m_copyFuture);
}

void InstantiateTemplateTask::copyFinished()
{
    if (!m_copyFuture.result()) {
        emitFailed(tr("Template folder copy failed."));
        return;
    }

    // Rewrite the staged instance.cfg so the new instance gets its own identity.
    // This mirrors what InstanceCopyTask does after copying.
    auto instanceSettings = std::make_unique<INISettingsObject>(FS::PathCombine(m_stagingPath, "instance.cfg"));
    std::unique_ptr<NullInstance> inst(new NullInstance(m_globalSettings, std::move(instanceSettings), m_stagingPath));

    inst->setName(name());
    inst->setIconKey(m_instIcon);
    inst->regenerateUuid();
    inst->resetTimePlayed();

    emitSucceeded();
}

void InstantiateTemplateTask::copyAborted()
{
    emitFailed(tr("Template folder copy has been aborted."));
}

bool InstantiateTemplateTask::abort()
{
    if (m_copyFutureWatcher.isRunning()) {
        m_copyFutureWatcher.cancel();
        // NOTE: Here we don't do `emitAborted()` because it will be done when `m_copyFutureWatcher` actually cancels, which may not occur
        // immediately.
        return true;
    }
    return false;
}
