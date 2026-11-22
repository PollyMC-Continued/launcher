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

#include "BedrockCreationPage.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRegularExpression>
#include <QVBoxLayout>

#include "icons/IconList.h"
#include "Application.h"
#include "BaseVersion.h"
#include "BedrockCreationTask.h"
#include "Filter.h"
#include "ui/dialogs/NewInstanceDialog.h"
#include "ui/widgets/VersionSelectWidget.h"

BedrockCreationPage::BedrockCreationPage(NewInstanceDialog* dialog, QWidget* parent)
    : QWidget(parent), m_dialog(dialog)
{
    m_list = BedrockVersionList::get();

    auto rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    /*
     * The version area, laid out like the Java "Custom" page: the version
     * list with the channel filter and the Refresh button on the right.
     */
    auto versionLayout = new QHBoxLayout();

    m_versionList = new VersionSelectWidget(this);
    m_versionList->setEmptyString(tr("No Bedrock versions match this filter. Press Refresh to fetch the version databases again."));
    m_versionList->setEmptyErrorString(tr("The Bedrock version list could not be loaded."));
    versionLayout->addWidget(m_versionList, 1);

    auto filterColumn = new QVBoxLayout();
    auto filterLabel = new QLabel(tr("Filter"), this);
    filterLabel->setAlignment(Qt::AlignCenter);
    filterColumn->addWidget(filterLabel);

    m_releasesFilter = new QCheckBox(tr("Releases"), this);
    m_releasesFilter->setChecked(true);
    filterColumn->addWidget(m_releasesFilter);

    m_previewsFilter = new QCheckBox(tr("Previews"), this);
    filterColumn->addWidget(m_previewsFilter);

    m_oldUwpFilter = new QCheckBox(tr("Old versions (legacy UWP)"), this);
    filterColumn->addWidget(m_oldUwpFilter);

    filterColumn->addStretch(1);

    m_refreshButton = new QPushButton(tr("Refresh"), this);
    m_refreshButton->setIcon(APPLICATION->icons()->getIcon("refresh"));
    filterColumn->addWidget(m_refreshButton);

    versionLayout->addLayout(filterColumn);
    rootLayout->addLayout(versionLayout, 1);

    /*
     * There are no mod loaders for Bedrock (packs are imported after creation),
     * so the version list takes the whole page. Only the "download on
     * creation" switch stays, as a single row below the list.
     */
    m_downloadOnCreate = new QCheckBox(tr("Download the game files on creation"), this);
    m_downloadOnCreate->setChecked(true);
    m_downloadOnCreate->setToolTip(tr("When unchecked the instance is created with the chosen version preselected, and the game "
                                      "package is installed on the first launch instead."));
    rootLayout->addWidget(m_downloadOnCreate);

    connect(m_versionList, &VersionSelectWidget::selectedVersionChanged, this, &BedrockCreationPage::suggestCurrent);
    connect(m_releasesFilter, &QCheckBox::stateChanged, this, &BedrockCreationPage::filterChanged);
    connect(m_previewsFilter, &QCheckBox::stateChanged, this, &BedrockCreationPage::filterChanged);
    connect(m_oldUwpFilter, &QCheckBox::stateChanged, this, &BedrockCreationPage::filterChanged);
    connect(m_refreshButton, &QPushButton::clicked, this, [this]() {
        if (!m_initialized) {
            m_versionList->initialize(m_list);
            m_initialized = true;
        }
        m_versionList->loadList(true);
    });
    connect(m_downloadOnCreate, &QCheckBox::toggled, this, &BedrockCreationPage::suggestCurrent);

    filterChanged();
}

void BedrockCreationPage::openedImpl()
{
    if (!m_initialized) {
        m_versionList->initialize(m_list);
        m_initialized = true;
    }
    /*
     * The initialize()/loadList() path preselects the recommended version, and
     * the selection change hands the matching creation task to the dialog.
     * Calling suggestCurrent() here as well covers the already-initialized
     * case (switching back to the Bedrock edition after the list has loaded).
     */
    suggestCurrent();
}

void BedrockCreationPage::retranslate()
{
    suggestCurrent();
}

void BedrockCreationPage::suggestCurrent()
{
    const BaseVersion::Ptr selected = m_versionList->selectedVersion();
    const BedrockVersion::Ptr version = std::dynamic_pointer_cast<BedrockVersion>(selected);
    if (!version || version->descriptor().isEmpty()) {
        m_dialog->setBedrockSuggestedPack();
        return;
    }
    m_dialog->setBedrockSuggestedPack(version->versionId(), new BedrockCreationTask(version, m_downloadOnCreate->isChecked()));
}

void BedrockCreationPage::filterChanged()
{
    /*
     * The three filters are disjoint, mirroring the Java page's
     * releases/snapshots/old checkboxes: releases, previews and the legacy
     * UWP-era versions. Each pattern is matched against the whole type column
     * so a checked filter never lets another category through. Nothing
     * checked shows everything.
     */
    QStringList patterns;
    if (m_releasesFilter->isChecked()) {
        patterns << QRegularExpression::escape(BedrockVersion::channelToString(BedrockVersion::Channel::Release));
    }
    if (m_previewsFilter->isChecked()) {
        patterns << QRegularExpression::escape(BedrockVersion::channelToString(BedrockVersion::Channel::Preview));
    }
    if (m_oldUwpFilter->isChecked()) {
        patterns << QRegularExpression::escape(BedrockVersion::legacyLabel());
    }
    const QString pattern = patterns.isEmpty() ? QStringLiteral(".*") : QStringLiteral("^(?:%1)$").arg(patterns.join(QLatin1Char('|')));
    m_versionList->setFilter(BaseVersionList::TypeRole, Filters::regexp(QRegularExpression(pattern)));
}
