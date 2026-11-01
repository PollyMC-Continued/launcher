// SPDX-License-Identifier: GPL-3.0-only
/*
 *  PollyMC-Continued - Minecraft Launcher
 *  Copyright (c) 2022 Jamie Mansfield <jmansfield@cadixdev.org>
 *  Copyright (C) 2023 TheKodeToad <TheKodeToad@proton.me>
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

#include <QListView>
#include <QModelIndex>
#include <QPointer>
#include <QWidget>

#include "ui/pages/BasePageContainer.h"
#include "ui/pages/BasePageProvider.h"

class QLayout;
class IconLabel;
class QSortFilterProxyModel;
class QStandardItemModel;
class PageModel;
class QLabel;
class QListView;
class QLineEdit;
class QStackedLayout;
class QGridLayout;

class PageContainer : public QWidget, public BasePageContainer {
    Q_OBJECT
   public:
    explicit PageContainer(BasePageProvider* pageProvider,
                           QString defaultId = QString(),
                           QWidget* parent = nullptr);
    ~PageContainer() override = default;

    void addButtons(QWidget* buttons);
    void addButtons(QLayout* buttons);

    void useSidebarStyle(bool sidebar);

    /*
     * Save any unsaved state and prepare to be closed.
     * @return true if everything can be saved, false if there is something that requires attention
     */
    bool prepareToClose();
    bool saveAll();

    /* request close - used by individual pages */
    bool requestClose() override
    {
        if (m_container) {
            return m_container->requestClose();
        }
        return false;
    }

    bool selectPage(QString pageId) override;
    BasePage* selectedPage() const override;
    BasePage* getPage(QString pageId) override;
    const QList<BasePage*>& getPages() const;

    void refreshContainer() override;
    virtual void setParentContainer(BasePageContainer* container) { m_container = container; };

    void changeEvent(QEvent*) override;

    void hidePageList()
    {
        m_pageList->hide();
        if (m_searchBox)
            m_searchBox->hide();
        if (m_searchResults)
            m_searchResults->hide();
    }

   private:
    void createUI();
    void retranslate();

   public slots:
    void help();

   signals:
    /** Emitted when the currently selected page is changed */
    void selectedPageChanged(BasePage* previous, BasePage* selected);

   private slots:
    void currentChanged(const QModelIndex& current);
    void showPage(int row);

    // Settings search (Ctrl+F): typing in the sidebar box replaces the page list with
    // matching settings; picking one jumps to its page and highlights the widget.
    void searchTextChanged(const QString& text);
    void searchReturnPressed();
    void searchResultActivated(const QModelIndex& index);

   private:
    void buildSearchIndex();
    void runSearch(const QString& query);
    void clearSearch();
    void jumpToResult(const QModelIndex& index);
    bool eventFilter(QObject* watched, QEvent* event) override;

    BasePageContainer* m_container = nullptr;
    BasePage* m_currentPage = 0;
    QSortFilterProxyModel* m_proxyModel;
    PageModel* m_model;
    QStackedLayout* m_pageStack;
    QListView* m_pageList;
    QLabel* m_header;
    QGridLayout* m_layout;

    // ---- settings search ----
    struct SearchEntry {
        QString label;     // what is shown and matched first
        QString keywords;  // tooltips, combo box items, ... matched at a lower weight, never shown
        QString section;   // "Tab › Group" the setting lives in
        QString pageId;
        QString pageName;
        QPointer<QWidget> widget;  // what gets revealed, focused and highlighted
        bool control = false;      // a real control (checkbox, labelled field, button) rather than loose text
        bool flash = true;
    };
    QLineEdit* m_searchBox = nullptr;
    QListView* m_searchResults = nullptr;
    QStandardItemModel* m_searchResultsModel = nullptr;
    QStringList m_queryTokens;  // lower-cased words of the current query, also used to highlight matches
    QList<SearchEntry> m_searchIndex;
    QList<int> m_activeResults;
    bool m_searchIndexBuilt = false;
    bool m_inSearch = false;
};
