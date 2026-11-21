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

#include "BedrockSettingsPage.h"

#include <QLabel>
#include <QVBoxLayout>

BedrockSettingsPage::BedrockSettingsPage(BedrockInstance* instance, QWidget* parent) : QWidget(parent), m_instance(instance)
{
    Q_UNUSED(m_instance)  // used by the backend settings added in later steps

    auto layout = new QVBoxLayout(this);
    m_infoLabel = new QLabel(this);
    m_infoLabel->setWordWrap(true);
    layout->addWidget(m_infoLabel);

    layout->addStretch(1);

    retranslate();
}

BedrockSettingsPage::~BedrockSettingsPage() {}

void BedrockSettingsPage::retranslate()
{
    m_infoLabel->setText(tr("Bedrock instances do not have Bedrock-specific settings yet.\n\n"
                            "The instance name, icon, group and notes can be changed from the launcher's instance menu.\n"
                            "Java-related settings do not apply to Bedrock instances."));
}
