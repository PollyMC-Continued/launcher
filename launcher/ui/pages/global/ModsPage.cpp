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

#include "ModsPage.h"
#include "ui_ModsPage.h"

#include "Application.h"
#include "SettingDescription.h"
#include "settings/SettingsObject.h"

ModsPage::ModsPage(QWidget* parent) : QWidget(parent), ui(new Ui::ModsPage)
{
    ui->setupUi(this);
    styleSettingDescriptions(this);
    loadSettings();
}

ModsPage::~ModsPage()
{
    delete ui;
}

bool ModsPage::apply()
{
    applySettings();
    return true;
}

void ModsPage::on_metadataEnableBtn_clicked()
{
    ui->metadataWarningLabel->setHidden(ui->metadataEnableBtn->isChecked());
}

void ModsPage::applySettings()
{
    auto s = APPLICATION->settings();

    s->set("ModMetadataDisabled", !ui->metadataEnableBtn->isChecked());
    s->set("ModDependenciesDisabled", !ui->dependenciesEnableBtn->isChecked());
    s->set("ShowModIncompat", ui->showModIncompatCheckBox->isChecked());
    s->set("SkipModpackUpdatePrompt", !ui->modpackUpdatePromptBtn->isChecked());
}

void ModsPage::loadSettings()
{
    auto s = APPLICATION->settings();

    ui->metadataEnableBtn->setChecked(!s->get("ModMetadataDisabled").toBool());
    ui->metadataWarningLabel->setHidden(ui->metadataEnableBtn->isChecked());
    ui->dependenciesEnableBtn->setChecked(!s->get("ModDependenciesDisabled").toBool());
    ui->showModIncompatCheckBox->setChecked(s->get("ShowModIncompat").toBool());
    ui->modpackUpdatePromptBtn->setChecked(!s->get("SkipModpackUpdatePrompt").toBool());
}

void ModsPage::retranslate()
{
    ui->retranslateUi(this);
}
