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

#include <QWidget>

#include "BedrockVersionList.h"
#include "ui/pages/BasePage.h"

class QCheckBox;
class QPushButton;
class NewInstanceDialog;
class VersionSelectWidget;

/*
 * The Bedrock page of the New Instance dialog.
 *
 * Shown when the Minecraft Edition selector is set to Bedrock, as the single
 * page of the Bedrock side's page container: the layout mirrors the Java
 * "Custom" page - the shared BedrockVersionList with the channel filter on
 * the right and the Refresh button, and, where the Java page has its mod
 * loader panel, the package panel (the selected version's package details and
 * the download-on-creation switch). There are no mod loaders and no modpack
 * providers for Bedrock in this phase, and CurseForge/Modrinth browsing is
 * deliberately out of scope.
 *
 * Picking a version immediately hands a BedrockCreationTask to the dialog.
 */
class BedrockCreationPage : public QWidget, public BasePage {
    Q_OBJECT
   public:
    explicit BedrockCreationPage(NewInstanceDialog* dialog, QWidget* parent = nullptr);
    ~BedrockCreationPage() override = default;

    ////// BasePage //////
    QString displayName() const override { return tr("Custom"); }
    QIcon icon() const override { return QIcon::fromTheme("minecraft"); }
    QString id() const override { return "bedrock"; }
    QString helpPage() const override { return QString(); }
    bool shouldDisplay() const override { return true; }
    void retranslate() override;

    void openedImpl() override;

   private:
    /* (Re-)hands the creation task matching the current selection to the dialog. */
    void suggestCurrent();
    /* Applies the filter checkboxes to the version list filter. */
    void filterChanged();

    NewInstanceDialog* m_dialog = nullptr;
    BedrockVersionList* m_list = nullptr;
    VersionSelectWidget* m_versionList = nullptr;
    QCheckBox* m_releasesFilter = nullptr;
    QCheckBox* m_previewsFilter = nullptr;
    QCheckBox* m_oldUwpFilter = nullptr;
    QPushButton* m_refreshButton = nullptr;
    QCheckBox* m_downloadOnCreate = nullptr;

    bool m_initialized = false;
};
