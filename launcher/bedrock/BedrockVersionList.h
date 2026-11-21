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

#include <QDateTime>
#include <QList>

#include "BedrockVersion.h"
#include "BaseVersionList.h"

/*
 * The Bedrock for Windows version list.
 *
 * Merges the community version databases verified in the Phase 0 report, all
 * through the launcher's existing Net stack and HttpMetaCache:
 *  - MinecraftBedrockArchiver/GdkLinks (urls.json): direct .msixvc package URLs
 *    for current GDK-era releases and previews. Primary for package URLs.
 *  - MinecraftBedrockArchiver/Metadata (w10_meta.json / w10_preview_meta.json):
 *    release history back to 0.12.0.1, file names and SHA-256 hashes, update
 *    ids. Primary for channels, hashes and history.
 *  - Rayth Network list (versions.php?type=json): UWP-era list with update ids.
 *    Mirror-only supplement.
 * The fe3 (Windows Update) refresh for current versions is not part of this
 * merge yet: it arrives together with the legacy UWP install pipeline that
 * actually consumes its download URLs.
 *
 * Resilience follows the proven pattern: on any failed refresh the previously
 * cached copies are kept (HttpMetaCache files survive failed downloads), and a
 * small snapshot shipped inside the launcher binary is the last resort, so the
 * list never goes silently empty. The dead Beta channel is never shown.
 */
class BedrockVersionList : public BaseVersionList {
    Q_OBJECT
   public:
    explicit BedrockVersionList(QObject* parent = nullptr);

    /*
     * The application-wide Bedrock version list (one instance, parented to the
     * application object so its lifetime follows the application).
     */
    static BedrockVersionList* get();

    ////// BaseVersionList //////
    Task::Ptr getLoadTask(bool forceReload = false) override;
    bool isLoaded() override;
    const BaseVersion::Ptr at(int i) const override;
    int count() const override;
    void sortVersions() override;
    BaseVersion::Ptr getRecommended() const override;

    ////// list model //////
    /*
     * Display: "1.26.65.1 — Release (current GDK package)". Tooltip: the
     * package details (file name, hash, URLs, update ids) the sources know.
     */
    QVariant data(const QModelIndex& index, int role) const override;

    ////// convenience //////
    /* Typed lookup by version id ("1.21.120.20"); nullptr when unknown. */
    BedrockVersion::Ptr findBedrockVersion(const QString& versionId) const;

    /*
     * Starts (or returns the already running) load task; never blocks. The
     * refreshSucceeded / refreshFailed signals report the outcome.
     */
    Task::Ptr refresh(bool force = false);

    /* Human-readable description of where the current data came from. */
    QString statusDescription() const;
    QDateTime lastRefreshed() const;
    bool loadedFromSnapshot() const { return m_fromSnapshot; }

   signals:
    void refreshSucceeded();
    void refreshFailed(const QString& reason);

   protected slots:
    void updateListData(QList<BaseVersion::Ptr> versions) override;

   private:
    friend class BedrockVersionListLoadTask;

    QString tooltipForVersion(const BedrockVersion::Ptr& version) const;

    QList<BedrockVersion::Ptr> m_versions;
    Task::Ptr m_currentLoadTask;
    bool m_loaded = false;
    bool m_fromSnapshot = false;
    QString m_statusDescription;
    QDateTime m_lastRefreshed;
};
