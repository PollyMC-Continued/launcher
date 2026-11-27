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

#include "FtbModpacksPage.h"

#include <QTabWidget>
#include <QVBoxLayout>

#include "FtbPage.h"
#include "../legacy_ftb/Page.h"

FtbModpacksPage::FtbModpacksPage(NewInstanceDialog* dialog, QWidget* parent)
    : QWidget(parent), m_currentPage(new FtbPage(dialog)), m_legacyPage(new LegacyFTB::Page(dialog))
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_tabs = new QTabWidget(this);
    m_tabs->addTab(m_currentPage, tr("Current"));
    m_tabs->addTab(m_legacyPage, tr("Legacy"));
    layout->addWidget(m_tabs);

    // switching tabs opens/closes the child pages, so their lazy loading and abort logic
    // keeps working as if they were pages of the container themselves
    connect(m_tabs, &QTabWidget::currentChanged, this, [this](int) {
        if (!isOpened)
            return;
        activePage()->closed();
        activePage()->opened();
    });
}

ModpackProviderBasePage* FtbModpacksPage::activePage() const
{
    return m_tabs->currentIndex() == 1 ? static_cast<ModpackProviderBasePage*>(m_legacyPage)
                                       : static_cast<ModpackProviderBasePage*>(m_currentPage);
}

void FtbModpacksPage::openedImpl()
{
    activePage()->opened();
}

void FtbModpacksPage::closedImpl()
{
    activePage()->closed();
}

void FtbModpacksPage::retranslate()
{
    m_currentPage->retranslate();
    m_legacyPage->retranslate();
    m_tabs->setTabText(0, tr("Current"));
    m_tabs->setTabText(1, tr("Legacy"));
}

void FtbModpacksPage::setSearchTerm(QString term)
{
    m_currentPage->setSearchTerm(term);
    m_legacyPage->setSearchTerm(term);
}

QString FtbModpacksPage::getSerachTerm() const
{
    return activePage()->getSerachTerm();
}
