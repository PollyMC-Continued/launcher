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

#include "AdvancedPage.h"
#include "ui_AdvancedPage.h"

#include "Application.h"
#include "settings/SettingsObject.h"

AdvancedPage::AdvancedPage(QWidget* parent) : QWidget(parent), ui(new Ui::AdvancedPage)
{
    ui->setupUi(this);
    loadSettings();
}

AdvancedPage::~AdvancedPage()
{
    delete ui;
}

bool AdvancedPage::apply()
{
    applySettings();
    return true;
}

void AdvancedPage::applySettings()
{
    auto s = APPLICATION->settings();

    s->set("NumberOfConcurrentTasks", ui->numberOfConcurrentTasksSpinBox->value());
    s->set("NumberOfConcurrentDownloads", ui->numberOfConcurrentDownloadsSpinBox->value());
    s->set("NumberOfManualRetries", ui->numberOfManualRetriesSpinBox->value());
    s->set("RequestTimeout", ui->timeoutSecondsSpinBox->value());

    // Console settings
    s->set("ConsoleMaxLines", ui->lineLimitSpinBox->value());
    s->set("ConsoleOverflowStop", ui->checkStopLogging->checkState() != Qt::Unchecked);
}

void AdvancedPage::loadSettings()
{
    auto s = APPLICATION->settings();

    ui->numberOfConcurrentTasksSpinBox->setValue(s->get("NumberOfConcurrentTasks").toInt());
    ui->numberOfConcurrentDownloadsSpinBox->setValue(s->get("NumberOfConcurrentDownloads").toInt());
    ui->numberOfManualRetriesSpinBox->setValue(s->get("NumberOfManualRetries").toInt());
    ui->timeoutSecondsSpinBox->setValue(s->get("RequestTimeout").toInt());

    // Console settings
    ui->lineLimitSpinBox->setValue(s->get("ConsoleMaxLines").toInt());
    ui->checkStopLogging->setChecked(s->get("ConsoleOverflowStop").toBool());
}

void AdvancedPage::retranslate()
{
    ui->retranslateUi(this);
}
