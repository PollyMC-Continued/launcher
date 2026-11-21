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

#include "BedrockInstance.h"
#include "ui/pages/BasePage.h"

class QLabel;

/*
 * Minimal "Version" page for Bedrock instances.
 *
 * TODO(next steps): the version list (releases / previews / old versions with a
 * channel filter), the creation dialog entry and the install/update tasks are
 * implemented after the scaffolding step; this page then grows a version list
 * and buttons like the Java VersionPage.
 */
class BedrockVersionPage : public QWidget, public BasePage {
    Q_OBJECT
   public:
    explicit BedrockVersionPage(BedrockInstance* instance, QWidget* parent = nullptr);
    virtual ~BedrockVersionPage();

    virtual QString displayName() const override { return tr("Version"); }
    virtual QIcon icon() const override;
    virtual QString id() const override { return "version"; }
    virtual QString helpPage() const override { return "Instance-Version"; }
    void retranslate() override;
    void openedImpl() override;

   private:
    void updateVersionInfo();

    BedrockInstance* m_instance;
    QLabel* m_versionLabel = nullptr;
    QLabel* m_noteLabel = nullptr;
};
