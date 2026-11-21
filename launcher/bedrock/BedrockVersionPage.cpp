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

#include "BedrockVersionPage.h"

#include <QLabel>
#include <QVBoxLayout>
#include "icons/IconList.h"   
#include "Application.h"

BedrockVersionPage::BedrockVersionPage(BedrockInstance* instance, QWidget* parent) : QWidget(parent), m_instance(instance)
{
    auto layout = new QVBoxLayout(this);
    m_versionLabel = new QLabel(this);
    m_versionLabel->setWordWrap(true);
    layout->addWidget(m_versionLabel);

    m_noteLabel = new QLabel(this);
    m_noteLabel->setWordWrap(true);
    layout->addWidget(m_noteLabel);

    layout->addStretch(1);

    retranslate();
    updateVersionInfo();
}

BedrockVersionPage::~BedrockVersionPage() {}

QIcon BedrockVersionPage::icon() const
{
    return APPLICATION->icons()->getIcon(m_instance->iconKey());
}

void BedrockVersionPage::updateVersionInfo()
{
    const auto version = m_instance->bedrockVersion();
    if (version.isEmpty()) {
        m_versionLabel->setText(tr("No Bedrock version has been selected for this instance yet."));
    } else {
        m_versionLabel->setText(tr("Bedrock version: %1").arg(version));
    }
}

void BedrockVersionPage::retranslate()
{
    // TODO(next steps): this note goes away once the version list and install
    // pipeline are wired up.
    m_noteLabel->setText(tr("Version selection and installation for Bedrock instances are not enabled yet."));
    updateVersionInfo();
}

void BedrockVersionPage::openedImpl()
{
    updateVersionInfo();
}
