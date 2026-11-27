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

#include "ui/pages/modplatform/ModpackProviderBasePage.h"

class NewInstanceDialog;
class QTabWidget;

namespace LegacyFTB {
class Page;
}
class FtbPage;

/**
 * The merged FTB page of NewInstanceDialog: the FTB (modpacks.ch API) page and the FTB Legacy
 * page live in its "Current" / "Legacy" tabs. Both pages keep their own code paths untouched;
 * this wrapper only forwards opened()/closed() and the search term of ModpackProviderBasePage.
 */
class FtbModpacksPage : public QWidget, public ModpackProviderBasePage {
    Q_OBJECT

   public:
    explicit FtbModpacksPage(NewInstanceDialog* dialog, QWidget* parent = nullptr);
    ~FtbModpacksPage() override = default;

    QString displayName() const override { return "FTB"; }
    QIcon icon() const override { return QIcon::fromTheme("ftb_logo"); }
    QString id() const override { return "ftb"; }
    QString helpPage() const override { return "FTB-platform"; }
    bool shouldDisplay() const override { return true; }

    QString sectionName() const override { return "Browse modpacks"; }

    void openedImpl() override;
    void closedImpl() override;
    void retranslate() override;

    /** Programatically set the term in the search bar. */
    virtual void setSearchTerm(QString) override;
    /** Get the current term in the search bar. */
    virtual QString getSerachTerm() const override;

   private:
    ModpackProviderBasePage* activePage() const;

    QTabWidget* m_tabs = nullptr;
    FtbPage* m_currentPage = nullptr;       // the FTB (current) modpacks
    LegacyFTB::Page* m_legacyPage = nullptr;  // the legacy FTB modpacks
};
