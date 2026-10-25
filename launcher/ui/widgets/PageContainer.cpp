// SPDX-License-Identifier: GPL-3.0-only
/*
 *  PollyMC-Continued - Minecraft Launcher
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
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

#include "PageContainer.h"
#include "BuildConfig.h"
#include "PageContainer_p.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QEvent>
#include <QGridLayout>
#include <QGroupBox>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QShortcut>
#include <QSortFilterProxyModel>
#include <QStackedLayout>
#include <QStringListModel>
#include <QStyledItemDelegate>
#include <QTabWidget>
#include <QTimer>
#include <QUrl>

#include <algorithm>

#include "settings/SettingsObject.h"

#include "ui/widgets/IconLabel.h"

#include "Application.h"
#include "DesktopServices.h"

namespace {
// Scrolls every scroll area between the widget and the window so the widget is actually visible.
void scrollWidgetIntoView(QWidget* target)
{
    QWidget* child = target;
    while (child) {
        QWidget* parent = child->parentWidget();
        if (auto* area = qobject_cast<QScrollArea*>(parent)) {
            const QPoint pos = child->mapTo(area->widget(), QPoint(0, 0));
            const int margin = 24;
            const int targetY = qMax(0, pos.y() - area->viewport()->height() / 3);
            area->verticalScrollBar()->setValue(qMin(targetY + margin, area->verticalScrollBar()->maximum()));
            const int targetX = qMax(0, pos.x() - area->viewport()->width() / 3);
            area->horizontalScrollBar()->setValue(qMin(targetX + margin, area->horizontalScrollBar()->maximum()));
        }
        child = parent;
    }
}

// Brief highlight so the found setting is easy to spot after the jump.
void flashWidget(QWidget* widget)
{
    if (!widget)
        return;
    const QString oldStyleSheet = widget->styleSheet();
    const QColor color = widget->palette().color(QPalette::Highlight);
    widget->setStyleSheet(oldStyleSheet + QStringLiteral("; background-color: rgba(%1, %2, %3, 70); border-radius: 3px;")
                              .arg(color.red())
                              .arg(color.green())
                              .arg(color.blue()));
    QTimer::singleShot(2400, widget, [widget, oldStyleSheet] {
        if (widget)
            widget->setStyleSheet(oldStyleSheet);
    });
}
}  // namespace

class PageEntryFilterModel : public QSortFilterProxyModel {
   public:
    explicit PageEntryFilterModel(QObject* parent = 0) : QSortFilterProxyModel(parent) {}

   protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
    {
        const QString pattern = filterRegularExpression().pattern();
        const auto model = static_cast<PageModel*>(sourceModel());
        const auto page = model->pages().at(sourceRow);
        if (!page->shouldDisplay())
            return false;
        // Regular contents check, then check page-filter.
        return QSortFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent);
    }
};

PageContainer::PageContainer(BasePageProvider* pageProvider, QString defaultId, QWidget* parent) : QWidget(parent)
{
    createUI();
    useSidebarStyle(true);

    m_model = new PageModel(this);
    m_proxyModel = new PageEntryFilterModel(this);
    int counter = 0;
    auto pages = pageProvider->getPages();
    for (auto page : pages) {
        auto widget = dynamic_cast<QWidget*>(page);
        widget->setParent(this);
        page->stackIndex = m_pageStack->addWidget(widget);
        page->listIndex = counter;
        page->setParentContainer(this);
        counter++;
        page->updateExtraInfo = [this](QString id, QString info) {
            if (m_currentPage && id == m_currentPage->id())
                m_header->setText(m_currentPage->displayName() + info);
        };
    }
    m_model->setPages(pages);

    m_proxyModel->setSourceModel(m_model);
    m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);

    m_pageList->setIconSize(QSize(pageIconSize, pageIconSize));
    m_pageList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_pageList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_pageList->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    m_pageList->setModel(m_proxyModel);
    connect(m_pageList->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &PageContainer::currentChanged);
    m_pageStack->setStackingMode(QStackedLayout::StackOne);
    m_pageList->setFocus();
    selectPage(defaultId);

    // Ctrl+F jumps to the search box, like the command palette shortcut in the main window.
    auto* findShortcut = new QShortcut(QKeySequence(QKeySequence::Find), this);
    connect(findShortcut, &QShortcut::activated, this, [this] {
        m_searchBox->setFocus();
        m_searchBox->selectAll();
    });
}

bool PageContainer::selectPage(QString pageId)
{
    // now find what we want to have selected...
    auto page = m_model->findPageEntryById(pageId);
    QModelIndex index;
    if (page) {
        index = m_proxyModel->mapFromSource(m_model->index(page->listIndex));
    }
    if (!index.isValid()) {
        index = m_proxyModel->index(0, 0);
    }
    if (index.isValid()) {
        m_pageList->setCurrentIndex(index);
        return true;
    }
    return false;
}

BasePage* PageContainer::getPage(QString pageId)
{
    return m_model->findPageEntryById(pageId);
}

BasePage* PageContainer::selectedPage() const
{
    return m_currentPage;
}

const QList<BasePage*>& PageContainer::getPages() const
{
    return m_model->pages();
}

void PageContainer::refreshContainer()
{
    m_proxyModel->invalidate();
    if (!m_currentPage->shouldDisplay()) {
        auto index = m_proxyModel->index(0, 0);
        if (index.isValid()) {
            m_pageList->setCurrentIndex(index);
        } else {
            // FIXME: unhandled corner case: what to do when there's no page to select?
        }
    }
}

void PageContainer::createUI()
{
    m_pageStack = new QStackedLayout;
    m_pageList = new PageView;
    m_header = new QLabel();

    QFont headerLabelFont = m_header->font();
    headerLabelFont.setBold(true);
    const int pointSize = headerLabelFont.pointSize();
    if (pointSize > 0)
        headerLabelFont.setPointSize(pointSize + 2);
    m_header->setFont(headerLabelFont);

    QHBoxLayout* headerHLayout = new QHBoxLayout;
    const int leftMargin = APPLICATION->style()->pixelMetric(QStyle::PM_LayoutLeftMargin);
    headerHLayout->addSpacerItem(new QSpacerItem(leftMargin, 0, QSizePolicy::Fixed, QSizePolicy::Ignored));
    headerHLayout->addWidget(m_header);
    headerHLayout->setContentsMargins(0, 6, 0, 0);

    m_pageStack->setContentsMargins(0, 0, 0, 0);
    m_pageStack->addWidget(new QWidget(this));

    // ---- settings search (Ctrl+F): a small box under the page list ----
    m_searchBox = new QLineEdit(this);
    m_searchBox->setClearButtonEnabled(true);
    m_searchBox->setPlaceholderText(tr("Search settings…"));
    m_searchBox->installEventFilter(this);
    connect(m_searchBox, &QLineEdit::textChanged, this, &PageContainer::searchTextChanged);
    connect(m_searchBox, &QLineEdit::returnPressed, this, &PageContainer::searchReturnPressed);

    m_searchResultsModel = new QStringListModel(this);
    m_searchResults = new QListView(this);
    m_searchResults->setModel(m_searchResultsModel);
    m_searchResults->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_searchResults->setUniformItemSizes(true);
    m_searchResults->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_searchResults->hide();
    m_searchResults->installEventFilter(this);
    connect(m_searchResults, &QListView::activated, this, &PageContainer::searchResultActivated);
    connect(m_searchResults, &QListView::clicked, this, &PageContainer::searchResultActivated);

    m_layout = new QGridLayout;
    m_layout->addLayout(headerHLayout, 0, 1, 1, 1);
    m_layout->addWidget(m_pageList, 0, 0, 3, 1);
    // results live in the same cell as the page list; only one is visible at a time
    m_layout->addWidget(m_searchResults, 0, 0, 3, 1);
    m_layout->addWidget(m_searchBox, 3, 0, 1, 1);
    m_layout->addLayout(m_pageStack, 1, 1, 1, 1);
    m_layout->setColumnStretch(1, 4);
    m_layout->setContentsMargins(0, 0, 0, 0);
    setLayout(m_layout);
}

void PageContainer::retranslate()
{
    if (m_currentPage)
        m_header->setText(m_currentPage->displayName());

    for (auto page : m_model->pages())
        page->retranslate();

    // the search index holds translated labels - rebuild it (lazily) on the next search
    m_searchIndexBuilt = false;
    m_searchIndex.clear();
    m_searchBox->setPlaceholderText(tr("Search settings…"));
}

void PageContainer::addButtons(QWidget* buttons)
{
    m_layout->addWidget(buttons, 2, 1, 1, 2);
}

void PageContainer::addButtons(QLayout* buttons)
{
    m_layout->addLayout(buttons, 2, 1, 1, 2);
}

void PageContainer::useSidebarStyle(bool sidebar)
{
    m_pageList->setProperty("_kde_side_panel_view", sidebar);
}

void PageContainer::showPage(int row)
{
    if (m_currentPage) {
        m_currentPage->closed();
    }
    if (row != -1) {
        m_currentPage = m_model->pages().at(row);
    } else {
        m_currentPage = nullptr;
    }
    if (m_currentPage) {
        m_pageStack->setCurrentIndex(m_currentPage->stackIndex);
        m_header->setText(m_currentPage->displayName());
        m_currentPage->opened();
    } else {
        m_pageStack->setCurrentIndex(0);
        m_header->setText(QString());
    }
}

void PageContainer::help()
{
    if (!m_currentPage)
        return;
    QString url;
    QString pageId = m_currentPage->helpPage();
    if (!pageId.isEmpty())
        url = BuildConfig.HELP_URL.arg(pageId);
    // this fork has no per-page help, so send the user to the Discord server instead
    if (url.isEmpty() || url == QLatin1String("https://"))
        url = BuildConfig.DISCORD_URL;
    if (!url.isEmpty())
        DesktopServices::openUrl(QUrl(url));
}

void PageContainer::currentChanged(const QModelIndex& current)
{
    int selected_index = current.isValid() ? m_proxyModel->mapToSource(current).row() : -1;

    auto* selected = m_model->pages().at(selected_index);
    auto* previous = m_currentPage;

    emit selectedPageChanged(previous, selected);

    showPage(selected_index);
}

bool PageContainer::prepareToClose()
{
    if (!saveAll()) {
        return false;
    }
    if (m_currentPage) {
        m_currentPage->closed();
    }
    return true;
}

bool PageContainer::saveAll()
{
    for (auto page : m_model->pages()) {
        if (!page->apply())
            return false;
    }
    return true;
}

void PageContainer::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslate();
    }
    QWidget::changeEvent(event);
}

void PageContainer::buildSearchIndex()
{
    m_searchIndex.clear();

    // a label is worth indexing if it looks like the name of a setting, not a paragraph of prose
    const auto usableText = [](QString text) -> QString {
        text = text.simplified();
        while (text.endsWith(QLatin1Char(':')))
            text.chop(1);
        if (text.isEmpty() || text.size() > 100)
            return {};
        if (text.contains('<') && text.contains('>'))  // rich text
            return {};
        if (text.startsWith("http:") || text.startsWith("https:"))
            return {};
        return text;
    };

    QSet<QString> seen;
    const auto addEntry = [this, &seen](const QString& label, BasePage* page, QWidget* widget) {
        if (label.isEmpty())
            return;
        const QString key = page->id() + QStringLiteral("|") + label;
        if (!seen.contains(key)) {
            seen.insert(key);
            m_searchIndex.append(SearchEntry{ label, page->id(), page->displayName(), widget });
        }
    };

    for (auto page : m_model->pages()) {
        if (!page->shouldDisplay())
            continue;
        auto* widget = dynamic_cast<QWidget*>(page);
        if (!widget)
            continue;

        for (auto* label : widget->findChildren<QLabel*>())
            addEntry(usableText(label->text()), page, label);
        for (auto* box : widget->findChildren<QCheckBox*>())
            addEntry(usableText(box->text()), page, box);
        for (auto* radio : widget->findChildren<QRadioButton*>())
            addEntry(usableText(radio->text()), page, radio);
        for (auto* group : widget->findChildren<QGroupBox*>())
            addEntry(usableText(group->title()), page, group);
        for (auto* tabs : widget->findChildren<QTabWidget*>())
            for (int i = 0; i < tabs->count(); ++i)
                addEntry(usableText(tabs->tabText(i)), page, tabs->widget(i));
    }

    m_searchIndexBuilt = true;
}

void PageContainer::searchTextChanged(const QString& text)
{
    const QString query = text.trimmed();
    if (query.isEmpty()) {
        clearSearch();
        return;
    }
    if (!m_searchIndexBuilt)
        buildSearchIndex();
    runSearch(query);
}

void PageContainer::runSearch(const QString& query)
{
    struct Scored {
        int index;
        int score;
    };
    QList<Scored> scored;
    for (int i = 0; i < m_searchIndex.size(); ++i) {
        const auto& entry = m_searchIndex.at(i);
        int score = 0;
        if (entry.label.startsWith(query, Qt::CaseInsensitive))
            score = 3;
        else if (entry.label.contains(query, Qt::CaseInsensitive))
            score = 2;
        else if (entry.pageName.contains(query, Qt::CaseInsensitive))
            score = 1;
        if (score > 0)
            scored.append({ i, score });
    }
    std::stable_sort(scored.begin(), scored.end(), [](const Scored& a, const Scored& b) { return a.score > b.score; });

    constexpr int MaxResults = 60;
    QStringList texts;
    m_activeResults.clear();
    for (int i = 0; i < scored.size() && i < MaxResults; ++i) {
        const auto& entry = m_searchIndex.at(scored.at(i).index);
        m_activeResults.append(scored.at(i).index);
        texts << QStringLiteral("%1 — %2").arg(entry.label, entry.pageName);
    }
    if (texts.isEmpty())
        texts << tr("No settings match \"%1\"").arg(query);

    m_searchResultsModel->setStringList(texts);
    m_pageList->hide();
    m_searchResults->show();
    m_inSearch = true;
    if (!m_activeResults.isEmpty())
        m_searchResults->setCurrentIndex(m_searchResultsModel->index(0, 0));
}

void PageContainer::clearSearch()
{
    if (!m_inSearch)
        return;
    m_inSearch = false;
    m_searchResults->hide();
    m_pageList->show();
    m_searchResultsModel->setStringList({});
}

void PageContainer::searchReturnPressed()
{
    auto index = m_searchResults->currentIndex();
    if (!index.isValid() && m_searchResultsModel->rowCount() > 0)
        index = m_searchResultsModel->index(0, 0);
    jumpToResult(index);
}

void PageContainer::searchResultActivated(const QModelIndex& index)
{
    jumpToResult(index);
}

void PageContainer::jumpToResult(const QModelIndex& index)
{
    if (!index.isValid())
        return;
    const int row = index.row();
    if (row < 0 || row >= m_activeResults.size())
        return;
    const SearchEntry entry = m_searchIndex.at(m_activeResults.at(row));

    // leaving search mode restores the regular page list
    m_searchBox->clear();  // triggers searchTextChanged -> clearSearch()
    selectPage(entry.pageId);

    // QPointer: the widget must survive until the page switch has landed
    QPointer<QWidget> safeTarget = entry.widget;
    QTimer::singleShot(0, this, [safeTarget] {
        QWidget* target = safeTarget;
        if (!target)
            return;
        scrollWidgetIntoView(target);
        if (target->focusPolicy() != Qt::NoFocus)
            target->setFocus(Qt::ShortcutFocusReason);
        flashWidget(target);
    });
}

bool PageContainer::eventFilter(QObject* watched, QEvent* event)
{
    if ((watched == m_searchBox || watched == m_searchResults) && event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
            // Esc leaves search mode instead of closing the whole dialog
            if (!m_searchBox->text().isEmpty()) {
                m_searchBox->clear();
            } else {
                clearSearch();
                m_pageList->setFocus();
            }
            return true;
        }
        if (watched == m_searchBox && keyEvent->key() == Qt::Key_Down && m_searchResults->isVisible() &&
            m_searchResultsModel->rowCount() > 0) {
            m_searchResults->setFocus();
            m_searchResults->setCurrentIndex(m_searchResultsModel->index(0, 0));
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}
