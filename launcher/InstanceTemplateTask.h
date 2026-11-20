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

#include <QFuture>
#include <QFutureWatcher>
#include <QString>
#include <QStringList>

#include "BaseInstance.h"
#include "Filter.h"
#include "InstanceTask.h"
#include "tasks/Task.h"

/**
 * Saves a copy of an instance into <dataRoot>/templates/<safe name> so it can be
 * used as a template for new instances.
 *
 * A template is a plain folder containing the instance's files (instance.cfg,
 * mmc-pack.json and the game folders allowed by the includeFolders whitelist),
 * plus a template.json manifest and an icon.png.
 *
 * Worlds (saves), logs, crash reports, screenshots and backups are never
 * included, no matter the options.
 *
 * Accepted includeFolders entries: "mods", "configs", "resourcepacks",
 * "shaderpacks" and "servers". Anything else in the whitelist is ignored.
 */
class SaveTemplateTask : public Task {
    Q_OBJECT
   public:
    explicit SaveTemplateTask(BaseInstance* source, QString templateName, QStringList includeFolders, QString iconKey, bool overwrite = false);

    /// Folder that holds every template: <dataRoot>/templates
    static QString templatesRoot();

    /// Strips characters that are not safe to use in a template folder name.
    static QString sanitizeTemplateDirName(const QString& name);

   protected:
    void executeTask() override;
    bool abort() override;

   private slots:
    void copyFinished();
    void copyAborted();

   private:
    QString buildExcludeRegex() const;

   private:
    BaseInstance* m_source;
    QString m_templateName;
    QStringList m_includeFolders;
    QString m_iconKey;
    bool m_overwrite;

    QString m_templateDir;
    QFuture<bool> m_copyFuture;
    QFutureWatcher<bool> m_copyFutureWatcher;
    Filter m_matcher;
};

/**
 * Turns a template folder into a new instance, using the InstanceList
 * staging/commit protocol: the task is meant to be wrapped with
 * InstanceList::wrapInstanceTask(), which provides the staging path (set through
 * InstanceTask::setStagingPath) and commits the staged folder into the instance
 * list once the task succeeds.
 *
 * After copying the template into the staging area, the staging instance.cfg is
 * rewritten so the resulting instance gets the requested name, group and icon,
 * a fresh UUID and a reset play time.
 */
class InstantiateTemplateTask : public InstanceTask {
    Q_OBJECT
   public:
    explicit InstantiateTemplateTask(QString templateDir, QString newInstanceName, QString group, QString iconKey);

   protected:
    void executeTask() override;
    bool abort() override;

   private slots:
    void copyFinished();
    void copyAborted();

   private:
    QString m_templateDir;
    QFuture<bool> m_copyFuture;
    QFutureWatcher<bool> m_copyFutureWatcher;
    Filter m_matcher;
};
