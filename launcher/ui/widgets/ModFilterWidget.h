// SPDX-License-Identifier: GPL-3.0-only
/*
 *  PollyMC-Continued - Minecraft Launcher
 *  Copyright (c) 2023 Trial97 <alexandru.tripon97@gmail.com>
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

#include <QButtonGroup>
#include <QHash>
#include <QList>
#include <QListWidgetItem>
#include <QTabWidget>
#include <utility>

#include "Version.h"

#include "VersionProxyModel.h"
#include "meta/VersionList.h"

#include "minecraft/MinecraftInstance.h"
#include "modplatform/ModIndex.h"

class MinecraftInstance;

class QCheckBox;

namespace Ui {
class ModFilterWidget;
}

class ModFilterWidget : public QTabWidget {
    Q_OBJECT
   public:
    struct Filter {
        std::vector<Version> versions;
        std::vector<ModPlatform::IndexedVersionType> releases;
        ModPlatform::ModLoaderTypes loaders;
        ModPlatform::Side side;
        bool hideInstalled;
        QStringList categoryIds;
        bool openSource;

        bool operator==(const Filter& other) const
        {
            return hideInstalled == other.hideInstalled && side == other.side && loaders == other.loaders && versions == other.versions &&
                   releases == other.releases && categoryIds == other.categoryIds && openSource == other.openSource;
        }
        bool operator!=(const Filter& other) const { return !(*this == other); }

        bool checkMcVersions(QStringList value)
        {
            for (auto mcVersion : versions)
                if (value.contains(mcVersion.toString()))
                    return true;

            return versions.empty();
        }

        bool checkModpackFilters(const ModPlatform::IndexedVersion& v)
        {
            return ((!loaders || !v.loaders || loaders & v.loaders) &&  // loaders
                    (releases.empty() ||                                // releases
                     std::find(releases.cbegin(), releases.cend(), v.version_type) != releases.cend()) &&
                    checkMcVersions({ v.mcVersion }));  // gameVersion}
        }
    };

    static std::unique_ptr<ModFilterWidget> create(MinecraftInstance* instance, bool extended);
    virtual ~ModFilterWidget();

    auto getFilter() -> std::shared_ptr<Filter>;
    auto changed() const -> bool { return m_filter_changed; }

    /** Read-only view of the current filter (unlike getFilter, does not reset the changed flag). */
    const Filter& currentFilter() const { return *m_filter; }

    /** Quick-filter hook used by the chip rows on the modpack pages: checks/unchecks the matching loader control. */
    void setLoaderChecked(ModPlatform::ModLoaderType loader, bool checked);

    /** The active categories as (display name, id) pairs, for the filter chips. */
    QList<QPair<QString, QString>> checkedCategories() const;

    /** Chip hook: remove one active Minecraft version (unchecks it in the version control). */
    void clearVersion(const QString& version);
    /** Chip hook: remove one active category. */
    void clearCategory(const QString& id);

   signals:
    void filterChanged();

   public slots:
    void setCategories(const QList<ModPlatform::Category>&);

   private:
    ModFilterWidget(MinecraftInstance* instance, bool extendedSupport);

    void loadVersionList();
    void prepareBasicFilter();

   private slots:
    void onVersionFilterChanged(int);
    void onVersionFilterTextChanged(const QString& version);
    void onLoadersFilterChanged();
    void onSideFilterChanged();
    void onHideInstalledFilterChanged();
    void onShowAllVersionsChanged();
    void onOpenSourceFilterChanged();
    void onReleaseFilterChanged();
    void onShowMoreClicked();

   private:
    Ui::ModFilterWidget* ui;

    MinecraftInstance* m_instance = nullptr;
    std::shared_ptr<Filter> m_filter;
    bool m_filter_changed = false;

    Meta::VersionList::Ptr m_version_list;
    VersionProxyModel* m_versions_proxy = nullptr;

    QList<ModPlatform::Category> m_categories;
    QHash<QString, QCheckBox*> m_categoryBoxes;  // id -> checkbox, so chips can uncheck one
};
