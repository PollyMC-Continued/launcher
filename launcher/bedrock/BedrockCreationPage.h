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

class NewInstanceDialog;
class QComboBox;
class QLabel;
class QPushButton;
class VersionSelectWidget;

/*
 * The Bedrock page of the New Instance dialog.
 *
 * Shown when the Minecraft Edition selector is set to Bedrock, in place of the
 * Java pages (version + mod loaders + the modpack providers): it is the shared
 * BedrockVersionList with the same channel filter the instance Version page
 * uses. Picking a version immediately hands a BedrockCreationTask to the
 * dialog - there are no mod loaders or modpack providers for Bedrock in this
 * phase, and CurseForge/Modrinth browsing is deliberately out of scope.
 *
 * A plain QWidget rather than a BasePage: it never sits in the page container,
 * the dialog switches to it as a whole through the edition stack.
 */
class BedrockCreationPage : public QWidget {
    Q_OBJECT
   public:
    explicit BedrockCreationPage(NewInstanceDialog* dialog, QWidget* parent = nullptr);
    ~BedrockCreationPage() override = default;

    /*
     * Called whenever the page becomes the active one (the dialog switched to
     * the Bedrock edition): loads the version list on first use. The list load
     * preselects the recommended version, which suggests the creation task.
     */
    void opened();

   private:
    /* (Re-)hands the creation task matching the current selection to the dialog. */
    void suggestCurrent();
    /* Applies the channel combo box selection to the version list filter. */
    void filterChanged();
    /* The platform-dependent hint text below the list. */
    void updateNoteLabel();

    NewInstanceDialog* m_dialog = nullptr;
    BedrockVersionList* m_list = nullptr;
    VersionSelectWidget* m_versionList = nullptr;
    QComboBox* m_channelFilter = nullptr;
    QPushButton* m_refreshButton = nullptr;
    QLabel* m_noteLabel = nullptr;

    bool m_initialized = false;
};
