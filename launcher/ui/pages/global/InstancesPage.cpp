// SPDX-License-Identifier: GPL-3.0-only
/*
 *  PollyMC-Continued - Minecraft Launcher
 *  Copyright (c) 2022 Jamie Mansfield <jmansfield@cadixdev.org>
 *  Copyright (c) 2022 dada513 <dada513@protonmail.com>
 *  Copyright (C) 2022 Tayou <git@tayou.org>
 *  Copyright (C) 2024 TheKodeToad <TheKodeToad@proton.me>
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

#include "InstancesPage.h"
#include "ui_InstancesPage.h"

#include "Application.h"
#include "SettingDescription.h"
#include "settings/SettingsObject.h"

// FIXME: possibly move elsewhere
enum InstSortMode {
    // Sort alphabetically by name.
    Sort_Name,
    // Sort by which instance was launched most recently.
    Sort_LastLaunch,
    // Sort by total time played, largest first.
    Sort_TimePlayed
};

InstancesPage::InstancesPage(QWidget* parent) : QWidget(parent), ui(new Ui::InstancesPage)
{
    ui->setupUi(this);

    ui->sortingModeGroup->setId(ui->sortByNameBtn, Sort_Name);
    ui->sortingModeGroup->setId(ui->sortLastLaunchedBtn, Sort_LastLaunch);
    ui->sortingModeGroup->setId(ui->sortByTimePlayedBtn, Sort_TimePlayed);

    styleSettingDescriptions(this);
    loadSettings();
}

InstancesPage::~InstancesPage()
{
    delete ui;
}

bool InstancesPage::apply()
{
    applySettings();
    return true;
}

void InstancesPage::applySettings()
{
    auto s = APPLICATION->settings();

    auto sortMode = (InstSortMode)ui->sortingModeGroup->checkedId();
    switch (sortMode) {
        case Sort_LastLaunch:
            s->set("InstSortMode", "LastLaunch");
            break;
        case Sort_TimePlayed:
            s->set("InstSortMode", "TimePlayed");
            break;
        case Sort_Name:
        default:
            s->set("InstSortMode", "Name");
            break;
    }

    if (ui->askToRenameDirBtn->isChecked()) {
        s->set("InstRenamingMode", "AskEverytime");
    } else if (ui->alwaysRenameDirBtn->isChecked()) {
        s->set("InstRenamingMode", "PhysicalDir");
    } else if (ui->neverRenameDirBtn->isChecked()) {
        s->set("InstRenamingMode", "MetadataOnly");
    }
}

void InstancesPage::loadSettings()
{
    auto s = APPLICATION->settings();

    QString sortMode = s->get("InstSortMode").toString();
    if (sortMode == "LastLaunch") {
        ui->sortLastLaunchedBtn->setChecked(true);
    } else if (sortMode == "TimePlayed") {
        ui->sortByTimePlayedBtn->setChecked(true);
    } else {
        ui->sortByNameBtn->setChecked(true);
    }

    QString renamingMode = s->get("InstRenamingMode").toString();
    ui->askToRenameDirBtn->setChecked(renamingMode == "AskEverytime");
    ui->alwaysRenameDirBtn->setChecked(renamingMode == "PhysicalDir");
    ui->neverRenameDirBtn->setChecked(renamingMode == "MetadataOnly");
}

void InstancesPage::retranslate()
{
    ui->retranslateUi(this);
}
