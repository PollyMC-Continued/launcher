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

#include <QAbstractButton>
#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QEvent>
#include <QGridLayout>
#include <QGroupBox>
#include <QHash>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPainter>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QShortcut>
#include <QSortFilterProxyModel>
#include <QStackedLayout>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QTabWidget>
#include <QTextDocument>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <utility>

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

constexpr int SecondaryRole = Qt::UserRole + 1;

// Removes "&" keyboard-mnemonic markers ("&Name" -> "Name", "Fish && Chips" -> "Fish & Chips").
QString stripMnemonic(QString text)
{
    text.replace(QStringLiteral("&&"), QStringLiteral("\x01"));
    text.remove(QLatin1Char('&'));
    text.replace(QLatin1Char('\x01'), QLatin1Char('&'));
    return text;
}

// True for widgets that were hidden on purpose (a feature that is turned off), not just not shown yet.
bool explicitlyHidden(const QWidget* widget, const QWidget* root)
{
    for (const QWidget* w = widget; w && w != root; w = w->parentWidget()) {
        if (w->testAttribute(Qt::WA_WState_ExplicitShowHide) && w->testAttribute(Qt::WA_WState_Hidden))
            return true;
    }
    return false;
}

// "Tab › Group" for a widget, outermost first.
QString sectionFor(const QWidget* widget, const QWidget* root)
{
    QStringList parts;
    const QWidget* child = widget;
    for (const QWidget* parent = widget->parentWidget(); parent && child != root; child = parent, parent = parent->parentWidget()) {
        if (auto* group = qobject_cast<const QGroupBox*>(parent)) {
            const QString title = stripMnemonic(group->title()).simplified();
            if (!title.isEmpty())
                parts.prepend(title);
        }
        if (auto* tabs = qobject_cast<const QTabWidget*>(parent->parentWidget())) {
            const int i = tabs->indexOf(const_cast<QWidget*>(child));
            if (i >= 0)
                parts.prepend(stripMnemonic(tabs->tabText(i)).simplified());
        }
    }
    parts.removeDuplicates();
    return parts.join(QStringLiteral(" › "));
}

// Makes a widget actually visible: switches tabs / stacked pages on the way up to the page.
void revealWidget(QWidget* target, const QWidget* stopAt)
{
    for (QWidget* child = target; child && child != stopAt; child = child->parentWidget()) {
        QWidget* parent = child->parentWidget();
        if (!parent)
            break;
        if (auto* tabs = qobject_cast<QTabWidget*>(parent->parentWidget())) {
            if (tabs->indexOf(child) >= 0)
                tabs->setCurrentWidget(child);
        } else if (auto* stack = qobject_cast<QStackedWidget*>(parent)) {
            stack->setCurrentWidget(child);
        }
    }
}

// Two-line result: the setting name with the typed words in bold, and "Page › Section" underneath.
class SearchResultDelegate : public QStyledItemDelegate {
   public:
    SearchResultDelegate(const QStringList* tokens, QObject* parent) : QStyledItemDelegate(parent), m_tokens(tokens) {}

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex&) const override
    {
        return QSize(0, QFontMetrics(option.font).height() * 2 + 10);
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        const QString primary = opt.text;
        const QString secondary = index.data(SecondaryRole).toString();
        opt.text.clear();
        const QWidget* widget = opt.widget;
        (widget ? widget->style() : QApplication::style())->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);

        const bool selected = opt.state & QStyle::State_Selected;
        const QColor mainColor = opt.palette.color(selected ? QPalette::HighlightedText : QPalette::Text);
        QColor subColor = mainColor;
        subColor.setAlphaF(selected ? 0.85 : 0.6);

        const QFontMetrics fm(opt.font);
        const QRect area = opt.rect.adjusted(8, 4, -8, -4);

        painter->save();
        painter->setClipRect(area);

        QTextDocument doc;
        doc.setDefaultFont(opt.font);
        doc.setDocumentMargin(0);
        doc.setHtml(QStringLiteral("<span style=\"color:%1\">%2</span>").arg(mainColor.name(), highlighted(primary)));
        painter->translate(area.topLeft());
        doc.drawContents(painter, QRectF(0, 0, area.width(), fm.height() + 2));
        painter->translate(-area.topLeft());

        if (!secondary.isEmpty()) {
            painter->setPen(subColor);
            const QRect subRect(area.left(), area.top() + fm.height() + 2, area.width(), fm.height());
            painter->drawText(subRect, Qt::AlignLeft | Qt::AlignVCenter, fm.elidedText(secondary, Qt::ElideRight, subRect.width()));
        }
        painter->restore();
    }

   private:
    QString highlighted(const QString& text) const
    {
        QVector<bool> mask(text.size(), false);
        for (const QString& token : *m_tokens) {
            if (token.isEmpty())
                continue;
            for (int from = 0; (from = text.indexOf(token, from, Qt::CaseInsensitive)) >= 0; from += token.size())
                for (int i = from; i < from + token.size() && i < mask.size(); ++i)
                    mask[i] = true;
        }
        QString html;
        for (int i = 0; i < text.size();) {
            int j = i;
            while (j < text.size() && mask[j] == mask[i])
                ++j;
            const QString part = text.mid(i, j - i).toHtmlEscaped();
            html += mask[i] ? QStringLiteral("<b>%1</b>").arg(part) : part;
            i = j;
        }
        return html;
    }

    const QStringList* m_tokens;
};
}  // namespace

class PageEntryFilterModel : public QSortFilterProxyModel {
   public:
    explicit PageEntryFilterModel(QObject* parent = nullptr) : QSortFilterProxyModel(parent) {}

   protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override
    {
        const QString pattern = filterRegularExpression().pattern();
        auto* const model = static_cast<PageModel*>(sourceModel());
        auto* const page = model->pages().at(sourceRow);
        if (!page->shouldDisplay()) {
            return false;
        }
        // Regular contents check, then check page-filter.
        return QSortFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent);
    }
};

PageContainer::PageContainer(BasePageProvider* pageProvider, QString defaultId, QWidget* parent)
    : QWidget(parent)
    , m_proxyModel(new PageEntryFilterModel(this))
    , m_model(new PageModel(this))
{
    createUI();
    useSidebarStyle(true);

    int counter = 0;
    auto pages = pageProvider->getPages();
    for (auto* page : pages) {
        auto* widget = dynamic_cast<QWidget*>(page);
        widget->setParent(this);
        page->stackIndex = m_pageStack->addWidget(widget);
        page->listIndex = counter;
        page->setParentContainer(this);
        counter++;
        page->updateExtraInfo = [this](const QString& id, const QString& info) {
            if (m_currentPage && id == m_currentPage->id()) {
                m_header->setText(m_currentPage->displayName() + info);
            }
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
    selectPage(std::move(defaultId));

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
    auto* page = m_model->findPageEntryById(pageId);
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
    if (pointSize > 0) {
        headerLabelFont.setPointSize(pointSize + 2);
    }
    m_header->setFont(headerLabelFont);

    auto* headerHLayout = new QHBoxLayout;
    const int leftMargin = APPLICATION->style()->pixelMetric(QStyle::PM_LayoutLeftMargin);
    headerHLayout->addSpacerItem(new QSpacerItem(leftMargin, 0, QSizePolicy::Fixed, QSizePolicy::Ignored));
    headerHLayout->addWidget(m_header);
    headerHLayout->setContentsMargins(0, 6, 0, 0);

    m_pageStack->setContentsMargins(0, 0, 0, 0);
    m_pageStack->addWidget(new QWidget(this));

    // ---- settings search (Ctrl+F): a small box above the page list ----
    m_searchBox = new QLineEdit(this);
    m_searchBox->setClearButtonEnabled(true);
    m_searchBox->setPlaceholderText(tr("Search settings…"));
    m_searchBox->setToolTip(tr("Search all settings (Ctrl+F). Several words narrow the results down."));
    m_searchBox->installEventFilter(this);
    connect(m_searchBox, &QLineEdit::textChanged, this, &PageContainer::searchTextChanged);
    connect(m_searchBox, &QLineEdit::returnPressed, this, &PageContainer::searchReturnPressed);

    m_searchResultsModel = new QStandardItemModel(this);
    m_searchResults = new QListView(this);
    m_searchResults->setModel(m_searchResultsModel);
    m_searchResults->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_searchResults->setUniformItemSizes(true);
    m_searchResults->setItemDelegate(new SearchResultDelegate(&m_queryTokens, m_searchResults));
    m_searchResults->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_searchResults->hide();
    m_searchResults->installEventFilter(this);
    connect(m_searchResults, &QListView::activated, this, &PageContainer::searchResultActivated);
    connect(m_searchResults, &QListView::clicked, this, &PageContainer::searchResultActivated);

    m_layout = new QGridLayout;
    m_layout->addLayout(headerHLayout, 0, 1, 1, 1);
    // the search box sits at the top of the left column, level with the page header
    m_layout->addWidget(m_searchBox, 0, 0, 1, 1);
    m_layout->addWidget(m_pageList, 1, 0, 2, 1);
    // results live in the same cell as the page list; only one is visible at a time
    m_layout->addWidget(m_searchResults, 1, 0, 2, 1);
    m_layout->addLayout(m_pageStack, 1, 1, 1, 1);
    m_layout->setColumnStretch(1, 4);
    m_layout->setContentsMargins(0, 0, 0, 0);
    setLayout(m_layout);
}

void PageContainer::retranslate()
{
    if (m_currentPage) {
        m_header->setText(m_currentPage->displayName());
    }

    for (auto* page : m_model->pages()) {
        page->retranslate();
    }

    // the search index holds translated labels - rebuild it (lazily) on the next search
    m_searchIndexBuilt = false;
    m_searchIndex.clear();
    m_searchBox->setPlaceholderText(tr("Search settings…"));
    m_searchBox->setToolTip(tr("Search all settings (Ctrl+F). Several words narrow the results down."));
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
    int selectedIndex = current.isValid() ? m_proxyModel->mapToSource(current).row() : -1;

    auto* selected = m_model->pages().at(selectedIndex);
    auto* previous = m_currentPage;

    emit selectedPageChanged(previous, selected);

    showPage(selectedIndex);
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
    for (auto* page : m_model->pages()) {
        if (!page->apply()) {
            return false;
        }
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
        text = stripMnemonic(text).simplified();
        while (text.endsWith(QLatin1Char(':')))
            text.chop(1);
        if (text.isEmpty() || text.size() > 100)
            return {};
        if (text.contains('<') && text.contains('>'))  // rich text
            return {};
        if (text.startsWith("http:") || text.startsWith("https:"))
            return {};
        if (text == QStringLiteral("…") || text == QStringLiteral("..."))
            return {};
        return text;
    };

    QHash<QString, int> byKey;  // page + label -> index, so a control beats a plain label of the same name
    const auto addEntry = [&](SearchEntry entry, BasePage* page, const QWidget* pageWidget) {
        if (entry.label.isEmpty() || !entry.widget || explicitlyHidden(entry.widget, pageWidget))
            return;
        entry.pageId = page->id();
        entry.pageName = page->displayName();
        entry.section = sectionFor(entry.widget, pageWidget);
        const QString key = entry.pageId + QLatin1Char('|') + entry.label.toLower();
        const auto it = byKey.constFind(key);
        if (it == byKey.constEnd()) {
            byKey.insert(key, m_searchIndex.size());
            m_searchIndex.append(entry);
        } else if (entry.control && !m_searchIndex.at(*it).control) {
            m_searchIndex[*it] = entry;
        }
    };

    // everything a user might type that is not the label: tooltips and the choices of a drop-down
    const auto keywordsFor = [](const QWidget* widget) {
        QStringList words;
        if (widget) {
            words << stripMnemonic(widget->toolTip()).simplified();
            if (auto* combo = qobject_cast<const QComboBox*>(widget))
                for (int i = 0; i < combo->count() && i < 40; ++i)
                    words << combo->itemText(i);
        }
        words.removeAll(QString());
        return words.join(QLatin1Char(' '));
    };

    for (auto page : m_model->pages()) {
        if (!page->shouldDisplay())
            continue;
        auto* pageWidget = dynamic_cast<QWidget*>(page);
        if (!pageWidget)
            continue;

        // Field captions point at their field through the buddy: search finds the field itself, with its
        // tooltip and choices as extra keywords.
        for (auto* label : pageWidget->findChildren<QLabel*>()) {
            SearchEntry entry;
            entry.label = usableText(label->text());
            QWidget* buddy = label->buddy();
            entry.widget = buddy ? buddy : label;
            entry.control = buddy != nullptr;
            entry.keywords = keywordsFor(buddy ? buddy : label);
            addEntry(entry, page, pageWidget);
        }
        for (auto* box : pageWidget->findChildren<QAbstractButton*>()) {
            const bool isToggle = qobject_cast<QCheckBox*>(box) || qobject_cast<QRadioButton*>(box);
            const bool isAction = qobject_cast<QPushButton*>(box) || qobject_cast<QToolButton*>(box);
            if (!isToggle && !isAction)
                continue;
            SearchEntry entry;
            entry.label = usableText(box->text());
            entry.widget = box;
            entry.control = true;
            entry.keywords = keywordsFor(box);
            addEntry(entry, page, pageWidget);
        }
        for (auto* group : pageWidget->findChildren<QGroupBox*>()) {
            SearchEntry entry;
            entry.label = usableText(group->title());
            entry.widget = group;
            entry.keywords = keywordsFor(group);
            addEntry(entry, page, pageWidget);
        }
        for (auto* tabs : pageWidget->findChildren<QTabWidget*>()) {
            for (int i = 0; i < tabs->count(); ++i) {
                SearchEntry entry;
                entry.label = usableText(tabs->tabText(i));
                entry.widget = tabs->widget(i);
                entry.flash = false;  // the tab itself opens; flashing a whole tab page is just noise
                addEntry(entry, page, pageWidget);
            }
        }
    }

    m_searchIndexBuilt = true;
}

void PageContainer::searchTextChanged(const QString& text)
{
    const QString query = text.simplified();
    if (query.isEmpty()) {
        clearSearch();
        return;
    }
    if (!m_searchIndexBuilt)
        buildSearchIndex();
    runSearch(query);
}

namespace {
bool isWordStart(const QString& text, int pos)
{
    return pos == 0 || !text.at(pos - 1).isLetterOrNumber();
}

// Typed characters appear in the same order in the text ("jvmargs" in "JVM arguments"): forgiving, so lowest rank.
bool isSubsequence(const QString& needle, const QString& haystack)
{
    int h = 0;
    for (const QChar c : needle) {
        h = haystack.indexOf(c, h, Qt::CaseInsensitive);
        if (h < 0)
            return false;
        ++h;
    }
    return true;
}
}  // namespace

void PageContainer::runSearch(const QString& query)
{
    m_queryTokens = query.toLower().split(QLatin1Char(' '), Qt::SkipEmptyParts);

    struct Scored {
        int index;
        int score;
    };

    // Every word has to match somewhere (name, then keywords, then section / page). Words matching the name
    // and matching it early or at the start of a word score highest.
    const auto scoreEntry = [this](const SearchEntry& entry) -> int {
        const QString label = entry.label.toLower();
        int total = 0;
        for (const QString& token : std::as_const(m_queryTokens)) {
            int score = 0;
            const int pos = label.indexOf(token);
            if (label == token)
                score = 100;
            else if (pos == 0)
                score = 60;
            else if (pos > 0 && isWordStart(label, pos))
                score = 40;
            else if (pos > 0)
                score = 20;
            else if (entry.keywords.contains(token, Qt::CaseInsensitive))
                score = 8;
            else if (entry.section.contains(token, Qt::CaseInsensitive) || entry.pageName.contains(token, Qt::CaseInsensitive))
                score = 4;
            else
                return 0;
            total += score;
        }
        // real controls beat loose text; shorter names beat long ones that merely contain the words
        total += entry.control ? 6 : 0;
        total -= qMin(entry.label.size() / 8, 5);
        return qMax(total, 1);
    };

    QList<Scored> scored;
    for (int i = 0; i < m_searchIndex.size(); ++i) {
        if (const int score = scoreEntry(m_searchIndex.at(i)); score > 0)
            scored.append({ i, score });
    }

    // nothing matched strictly: forgive typos in the middle / skipped letters
    if (scored.isEmpty() && query.size() >= 3) {
        const QString squashed = QString(query).remove(QLatin1Char(' '));
        for (int i = 0; i < m_searchIndex.size(); ++i) {
            if (isSubsequence(squashed, m_searchIndex.at(i).label))
                scored.append({ i, 1 });
        }
    }

    std::stable_sort(scored.begin(), scored.end(), [](const Scored& a, const Scored& b) { return a.score > b.score; });

    constexpr int MaxResults = 60;
    m_searchResultsModel->clear();
    m_activeResults.clear();
    for (int i = 0; i < scored.size() && i < MaxResults; ++i) {
        const auto& entry = m_searchIndex.at(scored.at(i).index);
        m_activeResults.append(scored.at(i).index);
        auto* item = new QStandardItem(entry.label);
        QString where = entry.pageName;
        if (!entry.section.isEmpty())
            where += QStringLiteral(" › ") + entry.section;
        item->setData(where, SecondaryRole);
        m_searchResultsModel->appendRow(item);
    }
    if (m_activeResults.isEmpty()) {
        auto* item = new QStandardItem(tr("No settings match \"%1\"").arg(query));
        item->setData(tr("Try fewer or shorter words, or part of the setting's name."), SecondaryRole);
        item->setFlags(Qt::NoItemFlags);
        m_searchResultsModel->appendRow(item);
    }

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
    m_searchResultsModel->clear();
    m_queryTokens.clear();
}

void PageContainer::searchReturnPressed()
{
    auto index = m_searchResults->currentIndex();
    if (!index.isValid() && !m_activeResults.isEmpty())
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
    const bool flash = entry.flash;
    QTimer::singleShot(0, this, [this, safeTarget, flash] {
        QWidget* target = safeTarget;
        if (!target)
            return;
        revealWidget(target, this);  // switch to the right tab first, otherwise the target is invisible
        scrollWidgetIntoView(target);
        if (target->focusPolicy() != Qt::NoFocus)
            target->setFocus(Qt::ShortcutFocusReason);
        if (flash)
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
