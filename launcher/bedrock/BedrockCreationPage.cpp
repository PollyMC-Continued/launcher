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

#include <QComboBox>
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

namespace {

/*
 * Channel filter modes; the combo box item order matches this enum. The dead
 * Beta channel is never offered (it does not exist in the merged sources).
 */
enum class ChannelFilter { All = 0, Release = 1, Preview = 2, OldUwp = 3 };

}  // namespace

BedrockCreationPage::BedrockCreationPage(NewInstanceDialog* dialog, QWidget* parent) : QWidget(parent), m_dialog(dialog)
{
    m_list = BedrockVersionList::get();

    auto rootLayout = new QVBoxLayout(this);

    auto controlLayout = new QHBoxLayout();
    controlLayout->addWidget(new QLabel(tr("Channel:"), this));
    m_channelFilter = new QComboBox(this);
    // The item order matches the ChannelFilter enum.
    m_channelFilter->addItem(tr("All versions"));
    m_channelFilter->addItem(tr("Releases"));
    m_channelFilter->addItem(tr("Previews"));
    m_channelFilter->addItem(tr("Old versions (legacy UWP)"));
    controlLayout->addWidget(m_channelFilter);
    controlLayout->addStretch(1);
    m_refreshButton = new QPushButton(tr("Refresh"), this);
    m_refreshButton->setIcon(APPLICATION->icons()->getIcon("refresh"));
    controlLayout->addWidget(m_refreshButton);
    rootLayout->addLayout(controlLayout);

    m_versionList = new VersionSelectWidget(this);
    m_versionList->setEmptyString(tr("No Bedrock versions match this channel. Press Refresh to fetch the version databases again."));
    m_versionList->setEmptyErrorString(tr("The Bedrock version list could not be loaded."));
    rootLayout->addWidget(m_versionList, 1);

    m_noteLabel = new QLabel(this);
    m_noteLabel->setWordWrap(true);
    rootLayout->addWidget(m_noteLabel);

    connect(m_versionList, &VersionSelectWidget::selectedVersionChanged, this, &BedrockCreationPage::suggestCurrent);
    connect(m_channelFilter, &QComboBox::currentIndexChanged, this, [this](int index) {
        Q_UNUSED(index)
        filterChanged();
    });
    connect(m_refreshButton, &QPushButton::clicked, this, [this]() {
        if (!m_initialized) {
            m_versionList->initialize(m_list);
            m_initialized = true;
        }
        m_versionList->loadList(true);
    });

    /* Releases is the useful default view, matching the instance Version page. */
    m_channelFilter->setCurrentIndex(static_cast<int>(ChannelFilter::Release));
    filterChanged();
    updateNoteLabel();
}

void BedrockCreationPage::opened()
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

void BedrockCreationPage::suggestCurrent()
{
    const BaseVersion::Ptr version = m_versionList->selectedVersion();
    if (!version || version->descriptor().isEmpty()) {
        m_dialog->setBedrockSuggestedPack();
        return;
    }
    m_dialog->setBedrockSuggestedPack(version->descriptor(), new BedrockCreationTask(version->descriptor()));
}

void BedrockCreationPage::filterChanged()
{
    /*
     * The filter matches on the version type string ("Release (current GDK
     * package)" / "Preview" / "Release (old UWP package)"), which encodes both
     * the channel and the packaging - the same grouping the instance Version
     * page shows.
     */
    QRegularExpression pattern(QStringLiteral(".*"));
    switch (static_cast<ChannelFilter>(m_channelFilter->currentIndex())) {
        case ChannelFilter::Release:
            pattern = QRegularExpression(QStringLiteral("^Release"));
            break;
        case ChannelFilter::Preview:
            pattern = QRegularExpression(QStringLiteral("^Preview"));
            break;
        case ChannelFilter::OldUwp:
            pattern = QRegularExpression(QStringLiteral("old UWP package"));
            break;
        case ChannelFilter::All:
        default:
            break;
    }
    m_versionList->setFilter(BaseVersionList::TypeRole, Filters::regexp(pattern));
}

void BedrockCreationPage::updateNoteLabel()
{
#if defined(Q_OS_WIN)
    m_noteLabel->setText(tr(
        "Pick a Bedrock version and press OK: the instance is created with that version preselected. Downloading, installing and "
        "launching the game itself arrives with the Windows backend update of Bedrock support; a launch attempt meanwhile prints "
        "its pre-flight check in the console and says that the backend is missing.\n"
        "On Windows only one GDK-era version of the game can be installed at a time: switching an instance to another GDK version "
        "later reinstalls the shared game. Every Bedrock instance is the data profile (worlds, packs, settings) around it."));
#elif defined(Q_OS_LINUX)
    m_noteLabel->setText(tr(
        "Pick a Bedrock version and press OK: the instance is created with that version preselected. The Linux backend "
        "(Proton-based, experimental) arrives with a later update of Bedrock support; a launch attempt meanwhile prints its "
        "pre-flight check in the console and reports that the backend is missing."));
#else
    m_noteLabel->setText(tr(
        "Bedrock Edition instances can be created here, but Bedrock support is not available on this platform: the Windows backend "
        "arrives with the next updates of Bedrock support, and no macOS backend is planned."));
#endif
}
