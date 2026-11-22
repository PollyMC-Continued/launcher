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
#include <QFrame>
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

    auto separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    rootLayout->addWidget(separator);

    /*
     * The package area, where the Java page has its mod loader panel: on the
     * left the selected version's package details (plus the platform notes),
     * on the right the "Download the game files on creation" switch. There
     * are no mod loaders for Bedrock - packs are imported after creation -
     * and no modpack providers in this phase.
     */
    auto packageLayout = new QHBoxLayout();

    m_packageDetails = new QLabel(this);
    m_packageDetails->setWordWrap(true);
    m_packageDetails->setAlignment(Qt::AlignTop);
    packageLayout->addWidget(m_packageDetails, 1);

    auto packageColumn = new QVBoxLayout();
    auto packageLabel = new QLabel(tr("Package"), this);
    packageLabel->setAlignment(Qt::AlignCenter);
    packageColumn->addWidget(packageLabel);

    m_downloadOnCreate = new QCheckBox(tr("Download the game files on creation"), this);
    m_downloadOnCreate->setChecked(true);
    m_downloadOnCreate->setToolTip(tr("When unchecked the instance is created with the chosen version preselected, and the game "
                                      "package is installed on the first launch instead."));
    packageColumn->addWidget(m_downloadOnCreate);

    packageColumn->addStretch(1);
    packageLayout->addLayout(packageColumn);

    rootLayout->addLayout(packageLayout);

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
    updatePackageDetails(nullptr);
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
        updatePackageDetails(nullptr);
        return;
    }
    m_dialog->setBedrockSuggestedPack(version->versionId(), new BedrockCreationTask(version, m_downloadOnCreate->isChecked()));
    updatePackageDetails(version);
}

void BedrockCreationPage::filterChanged()
{
    /*
     * The three filters are disjoint, mirroring the Java page's
     * releases/snapshots/old checkboxes: current GDK releases, previews, and
     * the legacy UWP-era releases. Nothing checked shows everything.
     */
    QStringList patterns;
    if (m_releasesFilter->isChecked()) {
        patterns << QStringLiteral("current GDK package");
    }
    if (m_previewsFilter->isChecked()) {
        patterns << QStringLiteral("^Preview");
    }
    if (m_oldUwpFilter->isChecked()) {
        patterns << QStringLiteral("old UWP package");
    }
    const QString pattern = patterns.isEmpty() ? QStringLiteral(".*") : patterns.join(QLatin1Char('|'));
    m_versionList->setFilter(BaseVersionList::TypeRole, Filters::regexp(QRegularExpression(pattern)));
}

void BedrockCreationPage::updatePackageDetails(const BedrockVersion::Ptr& version)
{
    QStringList lines;

    if (version) {
        lines << tr("Packaging: %1").arg(version->typeString());
        if (!version->fileName().isEmpty()) {
            lines << tr("Package file: %1").arg(version->fileName());
        }
        lines << tr("SHA-256: %1").arg(version->hasSha256() ? tr("known - verified on download") : tr("unknown for this version"));
    } else {
        lines << tr("No Bedrock version is selected.");
    }

    lines << QString();
#if defined(Q_OS_WIN)
    lines << tr(
        "Pressing OK downloads and installs the game package of the selected version. Current versions install as Windows GDK "
        "packages through Windows itself, which requires a Microsoft account that owns the game; only one GDK version can be "
        "installed at a time, and switching to another one reinstalls the shared game (the launcher asks first). Versions up "
        "to 1.21.120.20 install as legacy UWP packages that need developer mode. Every Bedrock instance is the data profile "
        "(worlds, packs, settings) around the shared game; resource and behavior packs are imported after creation.");
#elif defined(Q_OS_LINUX)
    lines << tr(
        "Pressing OK stages the game package of the selected version. The first launch sets up the experimental GDK-Proton "
        "backend (the engine, umu and the licensing client are downloaded on first use, hash-pinned) and then runs the game. "
        "Requires a Vulkan 1.3-class driver, an internet connection at every game start, and a Microsoft account that owns the "
        "game. Achievements do not unlock in this backend.");
#else
    lines << tr(
        "Bedrock support is not available on this platform: the Windows backend needs Windows, the Linux backend needs Linux, "
        "and no macOS backend is planned.");
#endif

    m_packageDetails->setText(lines.join(QLatin1Char('\n')));
}
