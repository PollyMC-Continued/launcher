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

#include "LauncherPage.h"
#include "ui_LauncherPage.h"

#include <QMenuBar>
#include <QTextCharFormat>

#include "Application.h"
#include "DiscordRichPresence.h"
#include "SettingDescription.h"
#include "settings/SettingsObject.h"
#include "ui/themes/ITheme.h"
#include "ui/themes/ThemeManager.h"
#include "updater/ExternalUpdater.h"
#include "updater/PollyMCExternalUpdater.h"

#include <QApplication>
#include <QProcess>

LauncherPage::LauncherPage(QWidget* parent) : QWidget(parent), ui(new Ui::LauncherPage)
{
    ui->setupUi(this);

    styleSettingDescriptions(this);
    loadSettings();

#ifdef Q_OS_MACOS
    if (!APPLICATION->updater()) {
        // macOS builds ship without a Sparkle feed/signing setup, so the updater section
        // shows a static note instead of controls that would do nothing (see root CMakeLists)
        ui->autoUpdateCheckBox->hide();
        ui->updateIntervalLabel->hide();
        ui->updateIntervalSpinBox->hide();
        ui->updaterNotSupportedNote->show();
    }
#else
    ui->updateSettingsBox->setHidden(!APPLICATION->updater());
#endif
}

LauncherPage::~LauncherPage()
{
    delete ui;
}

bool LauncherPage::apply()
{
    applySettings();
    return true;
}

void LauncherPage::on_discordRPEnabledCheckBox_stateChanged(int state)
{
    const bool enabled = (state == Qt::Checked);
    APPLICATION->settings()->set("DiscordRPEnabled", enabled);
    auto* presence = DiscordRichPresence::instance();
    if (!enabled) {
        presence->shutdown();
    } else if (!presence->isIpcConnected() && !presence->isWsActive()) {
        presence->init();
    }
}

void LauncherPage::applySettings()
{
    auto s = APPLICATION->settings();

    // Updates
    if (APPLICATION->updater()) {
        APPLICATION->updater()->setAutomaticallyChecksForUpdates(ui->autoUpdateCheckBox->isChecked());
        APPLICATION->updater()->setUpdateCheckInterval(ui->updateIntervalSpinBox->value() * 3600);
        if (auto* pollymcUpdater = qobject_cast<PollyMCExternalUpdater*>(APPLICATION->updater()))
            pollymcUpdater->setAutoUpdateMode(ui->updateBehaviorComboBox->currentData().toString());
    }

    s->set("AutoLaunchLastInstance", ui->autoLaunchCheckBox->isChecked());
    s->set("MinimizeToTray", ui->minimizeToTrayCheckBox->isChecked());
    s->set("DiscordRPEnabled", ui->discordRPEnabledCheckBox->isChecked());

    s->set("MenuBarInsteadOfToolBar", ui->preferMenuBarCheckBox->isChecked());
}

void LauncherPage::loadSettings()
{
    auto s = APPLICATION->settings();
    // Updates
    if (APPLICATION->updater()) {
        ui->autoUpdateCheckBox->setChecked(APPLICATION->updater()->getAutomaticallyChecksForUpdates());
        ui->updateIntervalSpinBox->setValue(APPLICATION->updater()->getUpdateCheckInterval() / 3600);
        if (auto* pollymcUpdater = qobject_cast<PollyMCExternalUpdater*>(APPLICATION->updater())) {
            const int idx = ui->updateBehaviorComboBox->findData(pollymcUpdater->getAutoUpdateMode());
            ui->updateBehaviorComboBox->setCurrentIndex(idx >= 0 ? idx : 1);  // default Ask
            ui->updateIntervalSpinBox->setEnabled(pollymcUpdater->getAutoUpdateMode() != "Never");
            // Updates are mandatory and checked every 30 minutes, so these options are not configurable.
            ui->updateSettingsBox->setEnabled(false);
            ui->updateSettingsBox->setToolTip(tr("Updates are mandatory: the launcher checks at startup and every 30 minutes."));
        }
    }

    ui->autoLaunchCheckBox->setChecked(s->get("AutoLaunchLastInstance").toBool());
    ui->minimizeToTrayCheckBox->setChecked(s->get("MinimizeToTray").toBool());
    ui->discordRPEnabledCheckBox->setChecked(s->get("DiscordRPEnabled").toBool());

    ui->preferMenuBarCheckBox->setChecked(s->get("MenuBarInsteadOfToolBar").toBool());
}

void LauncherPage::retranslate()
{
    ui->retranslateUi(this);
}
