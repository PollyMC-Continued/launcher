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

#include <QComboBox>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QListView>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QVBoxLayout>

#include "Application.h"
#include "icons/IconList.h"

namespace {

/*
 * Filter modes for the version list. "Old versions" are the legacy UWP-era
 * packages (plain .Appx, the future extract-and-register pipeline); the dead
 * Beta channel is never shown anywhere.
 */
enum class ChannelFilter { All = 0, Release = 1, Preview = 2, OldUwp = 3 };

}  // namespace

/*
 * Channel filter in front of the shared version list. Filtering only: the
 * source model already sorts newest-first, so no proxy-side sorting happens.
 */
class BedrockVersionFilterModel : public QSortFilterProxyModel {
   public:
    explicit BedrockVersionFilterModel(BedrockVersionList* list, QObject* parent = nullptr)
        : QSortFilterProxyModel(parent), m_list(list)
    {
        setSourceModel(list);
    }

    void setChannelFilter(ChannelFilter filter)
    {
        m_filter = filter;
        invalidateFilter();
    }

   protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override
    {
        Q_UNUSED(sourceParent)
        if (m_filter == ChannelFilter::All) {
            return true;
        }
        const BaseVersion::Ptr version = m_list->at(sourceRow);
        auto bedrockVersion = std::dynamic_pointer_cast<BedrockVersion>(version);
        if (!bedrockVersion) {
            return false;
        }
        switch (m_filter) {
            case ChannelFilter::Release:
                return bedrockVersion->channel() == BedrockVersion::Channel::Release;
            case ChannelFilter::Preview:
                return bedrockVersion->channel() == BedrockVersion::Channel::Preview;
            case ChannelFilter::OldUwp:
                return bedrockVersion->isLegacyUwp();
            case ChannelFilter::All:
            default:
                return true;
        }
    }

   private:
    BedrockVersionList* m_list;
    ChannelFilter m_filter = ChannelFilter::All;
};

BedrockVersionPage::BedrockVersionPage(BedrockInstance* instance, QWidget* parent) : QWidget(parent), m_instance(instance)
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

    m_proxy = new BedrockVersionFilterModel(m_list, this);
    m_versionList = new QListView(this);
    m_versionList->setModel(m_proxy);
    m_versionList->setSelectionMode(QAbstractItemView::SingleSelection);
    rootLayout->addWidget(m_versionList, 1);

    m_currentLabel = new QLabel(this);
    m_currentLabel->setWordWrap(true);
    rootLayout->addWidget(m_currentLabel);

    m_sourceLabel = new QLabel(this);
    m_sourceLabel->setWordWrap(true);
    rootLayout->addWidget(m_sourceLabel);

    m_noteLabel = new QLabel(this);
    m_noteLabel->setWordWrap(true);
    rootLayout->addWidget(m_noteLabel);

    auto buttonLayout = new QHBoxLayout();
    m_selectButton = new QPushButton(tr("Select version"), this);
    m_selectButton->setEnabled(false);
    buttonLayout->addWidget(m_selectButton);
    buttonLayout->addStretch(1);
    rootLayout->addLayout(buttonLayout);

    connect(m_channelFilter, &QComboBox::currentIndexChanged, this, [this](int index) {
        m_proxy->setChannelFilter(static_cast<ChannelFilter>(index));
        updateSelectButton();
    });
    connect(m_refreshButton, &QPushButton::clicked, this, [this]() {
        m_list->refresh(true);
        updateStatusLabels();
    });
    connect(m_list, &BedrockVersionList::refreshSucceeded, this, [this]() {
        updateStatusLabels();
        highlightCurrentVersion();
    });
    connect(m_list, &BedrockVersionList::refreshFailed, this, [this](const QString& reason) {
        Q_UNUSED(reason)
        updateStatusLabels();
    });
    connect(m_versionList->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]() { updateSelectButton(); });
    connect(m_versionList, &QListView::doubleClicked, this, [this](const QModelIndex& index) {
        Q_UNUSED(index)
        selectVersion();
    });
    connect(m_selectButton, &QPushButton::clicked, this, [this]() { selectVersion(); });

    retranslate();

    /* Releases is the useful default view. */
    m_channelFilter->setCurrentIndex(static_cast<int>(ChannelFilter::Release));
    m_proxy->setChannelFilter(ChannelFilter::Release);

    updateStatusLabels();
}

BedrockVersionPage::~BedrockVersionPage() {}

QIcon BedrockVersionPage::icon() const
{
    return APPLICATION->icons()->getIcon(m_instance->iconKey());
}

void BedrockVersionPage::selectVersion()
{
    const BedrockVersion::Ptr version = selectedVersion();
    if (!version) {
        return;
    }
    m_instance->setBedrockVersion(version->versionId());
    updateStatusLabels();
}

void BedrockVersionPage::highlightCurrentVersion()
{
    const QString versionId = m_instance->bedrockVersion();
    if (versionId.isEmpty()) {
        m_versionList->selectionModel()->clearSelection();
        return;
    }
    const BedrockVersion::Ptr version = m_list->findBedrockVersion(versionId);
    if (!version) {
        return;
    }
    for (int row = 0; row < m_list->count(); row++) {
        const BaseVersion::Ptr candidate = m_list->at(row);
        auto bedrockCandidate = std::dynamic_pointer_cast<BedrockVersion>(candidate);
        if (bedrockCandidate && bedrockCandidate->versionId() == versionId) {
            const QModelIndex sourceIndex = m_list->index(row, 0);
            const QModelIndex proxyIndex = m_proxy->mapFromSource(sourceIndex);
            if (proxyIndex.isValid()) {
                m_versionList->setCurrentIndex(proxyIndex);
                m_versionList->scrollTo(proxyIndex);
            }
            return;
        }
    }
}

BedrockVersion::Ptr BedrockVersionPage::selectedVersion() const
{
    const QModelIndex proxyIndex = m_versionList->currentIndex();
    if (!proxyIndex.isValid()) {
        return nullptr;
    }
    const QModelIndex sourceIndex = m_proxy->mapToSource(proxyIndex);
    if (!sourceIndex.isValid()) {
        return nullptr;
    }
    auto version = std::dynamic_pointer_cast<BedrockVersion>(m_list->at(sourceIndex.row()));
    return version;
}

void BedrockVersionPage::updateStatusLabels()
{
    const QString versionId = m_instance->bedrockVersion();
    if (versionId.isEmpty()) {
        m_currentLabel->setText(tr("No game version is selected for this instance yet. Pick one from the list and press Select version."));
    } else {
        const BedrockVersion::Ptr version = m_list->findBedrockVersion(versionId);
        if (version) {
            m_currentLabel->setText(tr("Selected version: %1 (%2)").arg(versionId, version->typeString()));
        } else {
            m_currentLabel->setText(tr("Selected version: %1 (not in the version list)").arg(versionId));
        }
    }

    if (m_list->isLoaded()) {
        QString sourceText = m_list->statusDescription();
        if (m_list->loadedFromSnapshot()) {
            sourceText += QStringLiteral("\n") + tr("No version source was reachable: this is the shipped snapshot. Press Refresh when online.");
        }
        m_sourceLabel->setText(sourceText);
    } else {
        m_sourceLabel->setText(tr("Loading the Bedrock version list..."));
    }

    updateSelectButton();
}

void BedrockVersionPage::updateSelectButton()
{
    m_selectButton->setEnabled(selectedVersion() != nullptr);
}

void BedrockVersionPage::updateNoteLabel()
{
#if defined(Q_OS_WIN)
    m_noteLabel->setText(tr(
        "Pick a version and press Select version: the choice is stored on the instance and shown in the launcher.\n"
        "The missing game files are installed on the first launch (or right when the instance is created): current "
        "versions as Windows GDK packages through the entitled install, versions up to 1.21.120.20 as legacy UWP "
        "packages that need developer mode.\n"
        "Only one GDK-era version can be installed at a time: switching an instance to another GDK version reinstalls "
        "the shared game (the launcher asks first), and each instance is the data profile around it - its worlds, "
        "packs and settings are kept in its own folder and linked to the game on launch."));
#elif defined(Q_OS_LINUX)
    m_noteLabel->setText(tr(
        "Pick a version and press Select version: the choice is stored on the instance and shown in the launcher.\n"
        "The first launch sets up the experimental GDK-Proton backend (the engine, umu and the licensing client are "
        "downloaded on first use, hash-pinned) and then runs the game. Requires a Vulkan 1.3-class driver, internet at "
        "every game start and a Microsoft account that owns the game; achievements do not unlock. Legacy UWP versions "
        "are Windows-only."));
#else
    m_noteLabel->setText(tr("Bedrock Edition instances are not supported on this platform."));
#endif
}

void BedrockVersionPage::retranslate()
{
    // The combo box item order matches the ChannelFilter enum.
    m_channelFilter->blockSignals(true);
    m_channelFilter->setItemText(static_cast<int>(ChannelFilter::All), tr("All versions"));
    m_channelFilter->setItemText(static_cast<int>(ChannelFilter::Release), tr("Releases"));
    m_channelFilter->setItemText(static_cast<int>(ChannelFilter::Preview), tr("Previews"));
    m_channelFilter->setItemText(static_cast<int>(ChannelFilter::OldUwp), tr("Old versions (legacy UWP)"));
    m_channelFilter->blockSignals(false);

    m_refreshButton->setText(tr("Refresh"));
    m_selectButton->setText(tr("Select version"));
    updateNoteLabel();
    updateStatusLabels();
}

void BedrockVersionPage::openedImpl()
{
    if (!m_list->isLoaded()) {
        m_list->refresh();
    }
    updateStatusLabels();
    highlightCurrentVersion();
}
