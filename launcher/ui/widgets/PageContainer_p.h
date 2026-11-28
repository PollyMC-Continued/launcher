/* Copyright 2026 PollyMC-Continued Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <QEvent>
#include <QFont>
#include <QFontMetrics>
#include <QListView>
#include <QPainter>
#include <QPalette>
#include <QScrollBar>
#include <QPixmap>
#include <QStyledItemDelegate>

class BasePage;
const int pageIconSize = 24;
// Custom model role: the sidebar heading a page belongs to (BasePage::sectionName()).
const int pageSectionRole = Qt::UserRole + 20;

class PageViewDelegate : public QStyledItemDelegate {
   public:
    PageViewDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        const QFontMetrics fm(option.font);
        const int textWidth = fm.horizontalAdvance(index.data(Qt::DisplayRole).toString());
        return QSize(textWidth + pageIconSize + 2 * SideMargin + 2 * PillPadding + IconGap, EntryHeight + sectionHeaderHeight(option, index));
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setRenderHint(QPainter::SmoothPixmapTransform);

        // section heading (only above the first page of a section)
        const int headerHeight = sectionHeaderHeight(option, index);
        if (headerHeight > 0) {
            QFont font = option.font;
            if (font.pointSizeF() > 1)
                font.setPointSizeF(font.pointSizeF() - 1);
            painter->setFont(font);
            painter->setPen(option.palette.color(QPalette::PlaceholderText));
            const QRect headerRect(option.rect.left() + SideMargin + PillPadding, option.rect.top(), option.rect.width() - 2 * SideMargin - 2 * PillPadding,
                                   headerHeight - 4);
            painter->drawText(headerRect, Qt::AlignLeft | Qt::AlignBottom, index.data(pageSectionRole).toString());
        }

        // the page entry: a rounded pill inset from the edges. Only the pill is highlighted, not the whole row.
        QRect entry = option.rect;
        entry.setTop(entry.top() + headerHeight);
        const QRect pill = entry.adjusted(SideMargin, 2, -SideMargin, -2);
        const bool selected = opt.state & QStyle::State_Selected;
        const bool hovered = opt.state & QStyle::State_MouseOver;
        // always the "active" colours: the sidebar should not turn grey when the list loses focus
        const QPalette& pal = opt.palette;
        if (selected || hovered) {
            QColor fill = selected ? pal.color(QPalette::Active, QPalette::Highlight) : pal.color(QPalette::Active, QPalette::Text);
            if (!selected)
                fill.setAlpha(26);
            painter->setPen(Qt::NoPen);
            painter->setBrush(fill);
            painter->drawRoundedRect(pill, PillRadius, PillRadius);
        }

        const QRect iconRect(pill.left() + PillPadding, pill.center().y() - pageIconSize / 2 + 1, pageIconSize, pageIconSize);
        if (!opt.icon.isNull()) {
            if (QIcon::themeName() == QLatin1String("pe_line")) {
                // monochrome theme: only the shape (alpha) of the SVG matters, the colour comes from the palette
                const bool darkUi = pal.color(QPalette::Window).lightness() < 128;
                QColor tint = darkUi ? QColor("#dfdfdf") : QColor("#2b2b2b");
                if (selected) {
                    // icon on the highlighted pill: dark on bright highlights (your green), white on dark ones
                    const bool brightHighlight = pal.color(QPalette::Active, QPalette::Highlight).lightness() > 128;
                    tint = brightHighlight ? QColor("#191919") : QColor("#ffffff");
                }
                QPixmap pixmap = opt.icon.pixmap(iconRect.size(), painter->device()->devicePixelRatioF());
                QPainter tinter(&pixmap);
                tinter.setCompositionMode(QPainter::CompositionMode_SourceIn);
                tinter.fillRect(pixmap.rect(), tint);
                tinter.end();
                painter->drawPixmap(iconRect, pixmap);
            } else {
                opt.icon.paint(painter, iconRect, Qt::AlignCenter, QIcon::Normal, QIcon::Off);
            }
        }

        const QRect textRect(iconRect.right() + 1 + IconGap, pill.top(), pill.right() - iconRect.right() - IconGap - PillPadding, pill.height());
        painter->setFont(option.font);
        painter->setPen(pal.color(QPalette::Active, selected ? QPalette::HighlightedText : QPalette::Text));
        painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine,
                          QFontMetrics(option.font).elidedText(opt.text, Qt::ElideRight, textRect.width()));
        painter->restore();
    }

   private:
    static constexpr int EntryHeight = 38;   // one page entry, including the gap between pills
    static constexpr int SideMargin = 8;     // space between the sidebar edge and the pill
    static constexpr int PillPadding = 10;   // space between the pill edge and its icon / text
    static constexpr int IconGap = 10;       // space between icon and text
    static constexpr int PillRadius = 8;

    // Extra height for the heading, only on the first visible page of each section.
    static int sectionHeaderHeight(const QStyleOptionViewItem& option, const QModelIndex& index)
    {
        const QString section = index.data(pageSectionRole).toString();
        if (section.isEmpty())
            return 0;
        if (index.row() > 0 && index.sibling(index.row() - 1, 0).data(pageSectionRole).toString() == section)
            return 0;
        return QFontMetrics(option.font).height() + 14;
    }
};

class PageModel : public QAbstractListModel {
   public:
    PageModel(QObject* parent = 0) : QAbstractListModel(parent)
    {
        QPixmap empty(pageIconSize, pageIconSize);
        empty.fill(Qt::transparent);
        m_emptyIcon = QIcon(empty);
    }
    virtual ~PageModel() {}

    int rowCount(const QModelIndex& parent = QModelIndex()) const { return parent.isValid() ? 0 : m_pages.size(); }
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const
    {
        switch (role) {
            case Qt::DisplayRole:
                return m_pages.at(index.row())->displayName();
            case pageSectionRole:
                return m_pages.at(index.row())->sectionName();
            case Qt::DecorationRole: {
                QIcon icon = m_pages.at(index.row())->icon();
                if (icon.isNull())
                    icon = m_emptyIcon;
                // HACK: fixes icon stretching on windows. TODO: report Qt bug for this
                return QIcon(icon.pixmap(QSize(48, 48)));
            }
        }
        return QVariant();
    }

    void setPages(const QList<BasePage*>& pages)
    {
        beginResetModel();
        m_pages = pages;
        endResetModel();
    }
    const QList<BasePage*>& pages() const { return m_pages; }

    BasePage* findPageEntryById(QString id)
    {
        for (auto page : m_pages) {
            if (page->id() == id)
                return page;
        }
        return nullptr;
    }

    QList<BasePage*> m_pages;
    QIcon m_emptyIcon;
};

class PageView : public QListView {
   public:
    PageView(QWidget* parent = 0) : QListView(parent)
    {
        setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Expanding);
        setItemDelegate(new PageViewDelegate(this));
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setMouseTracking(true);  // hover highlight of the sidebar pills
    }

    virtual QSize sizeHint() const
    {
        int width = sizeHintForColumn(0) + frameWidth() * 2 + 5 + 24 /* sidebar panel margins */;
        if (verticalScrollBar()->isVisible())
            width += verticalScrollBar()->width();
        return QSize(width, 100);
    }

    virtual bool eventFilter(QObject* obj, QEvent* event)
    {
        if (obj == verticalScrollBar() && (event->type() == QEvent::Show || event->type() == QEvent::Hide))
            updateGeometry();
        return QListView::eventFilter(obj, event);
    }
};
