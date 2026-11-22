// SPDX-License-Identifier: GPL-3.0-only
/*
 *  PollyMC-Continued - Minecraft Launcher
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3.
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

#include "BedrockInstance.h"
#include "ui/pages/BasePage.h"

class QCheckBox;
class QLabel;
class QLineEdit;

/*
 * The Settings page of a Bedrock instance.
 *
 * The data-profile behavior on Windows (separate game data per instance, the
 * optional launch-helper DLL that pins the exact installed GDK version) and
 * the advanced backend settings on Linux (a locally built licensing client,
 * the custom launch command, the Microsoft account token for the in-game
 * sign-in). Everything is optional and defaults to the automatic behavior;
 * nothing here affects Java instances.
 */
class BedrockSettingsPage : public QWidget, public BasePage {
    Q_OBJECT
   public:
    explicit BedrockSettingsPage(BedrockInstance* instance, QWidget* parent = nullptr);
    virtual ~BedrockSettingsPage();

    virtual QString displayName() const override { return tr("Settings"); }
    virtual QIcon icon() const override { return QIcon::fromTheme("instance-settings"); }
    virtual QString id() const override { return "settings"; }
    virtual QString helpPage() const override { return "Instance-settings"; }
    bool apply() override;
    void retranslate() override;

   private:
    BedrockInstance* m_instance;
    QLabel* m_infoLabel = nullptr;
#if defined(Q_OS_WIN)
    QCheckBox* m_separateGameData = nullptr;
    QLineEdit* m_launchHelperPath = nullptr;
#endif
#if defined(Q_OS_LINUX)
    QLineEdit* m_xodusPath = nullptr;
    QLineEdit* m_launchCommand = nullptr;
    QLineEdit* m_msaRefreshToken = nullptr;
#endif
};
