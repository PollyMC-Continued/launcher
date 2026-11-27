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

#include "ModpackItemDelegate.h"

#include <QFontMetrics>
#include <QHash>
#include <QPainter>
#include <QRegularExpression>

#include "ui/widgets/ProjectItem.h"

QString stripHtml(QString text)
{
    static const QRegularExpression tags("<[^>]*>");
    text.remove(tags);
    // unescape the common entities - &amp; last so it does not re-create the others
    text.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "));
    text.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
    text.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    text.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    text.replace(QStringLiteral("&apos;"), QStringLiteral("'"));
    text.replace(QStringLiteral("&#39;"), QStringLiteral("'"));
    text.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    return text.simplified();
}

ModpackItemDelegate::ModpackItemDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

QSize ModpackItemDelegate::sizeHint(const QStyleOptionViewItem& option, [[maybe_unused]] const QModelIndex& index) const
{
    const QFontMetrics fm(option.font);
    const int textHeight = fm.height() + 2 + fm.height() + 2 + fm.height() * 2 / 3;
    return QSize(0, qMax(64, textHeight + 12));
}

void ModpackItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setClipRect(opt.rect);

    const QPalette& pal = opt.palette;
    const bool selected = opt.state & QStyle::State_Selected;
    const bool hovered = opt.state & QStyle::State_MouseOver;

    // the row background is a rounded pill, the same language as the sidebar
    const QRect pill = opt.rect.adjusted(4, 1, -4, -1);
    if (selected || hovered) {
        QColor fill = selected ? pal.color(QPalette::Active, QPalette::Highlight) : pal.color(QPalette::Active, QPalette::Text);
        if (!selected)
            fill.setAlpha(26);
        painter->setPen(Qt::NoPen);
        painter->setBrush(fill);
        painter->drawRoundedRect(pill, 8, 8);
    }

    // icon, or a coloured tile with the pack's initial while there is no image
    const int tileSize = qMin(pill.height() - 10, 48);
    const QRect iconRect(pill.left() + 8, pill.top() + (pill.height() - tileSize) / 2, tileSize, tileSize);
    const QString title = index.data(UserDataTypes::TITLE).toString();

    if (!opt.icon.isNull()) {
        opt.icon.paint(painter, iconRect, Qt::AlignCenter, QIcon::Normal, QIcon::Off);
    } else {
        // a deterministic palette colour: themes stay consistent, packs get varied tiles
        static const QPalette::ColorRole tileRoles[] = { QPalette::Highlight, QPalette::Link, QPalette::LinkVisited, QPalette::Mid };
        QColor tile = pal.color(tileRoles[qHash(title) % (sizeof(tileRoles) / sizeof(tileRoles[0]))]);
        tile.setAlpha(70);
        painter->setPen(Qt::NoPen);
        painter->setBrush(tile);
        painter->drawRoundedRect(iconRect, 8, 8);
        if (!title.isEmpty()) {
            QFont tileFont = opt.font;
            tileFont.setWeight(QFont::DemiBold);
            painter->setFont(tileFont);
            painter->setPen(pal.color(QPalette::Active, QPalette::WindowText));
            painter->drawText(iconRect, Qt::AlignCenter, title.left(1).toUpper());
        }
    }

    // two-line text block, plus small metadata (downloads, loader, Minecraft version)
    const QRect text = pill.adjusted(iconRect.width() + 16, 4, -8, -4);
    const QColor main = pal.color(QPalette::Active, selected ? QPalette::HighlightedText : QPalette::Text);
    QColor muted = main;
    muted.setAlphaF(0.65);

    QFont titleFont = opt.font;
    titleFont.setWeight(QFont::DemiBold);
    QFont metaFont = opt.font;
    if (metaFont.pointSizeF() > 2.)
        metaFont.setPointSizeF(metaFont.pointSizeF() - 1.);
    const QFontMetrics titleFm(titleFont);
    const QFontMetrics textFm(opt.font);
    const QFontMetrics metaFm(metaFont);

    const QString description = stripHtml(index.data(UserDataTypes::DESCRIPTION).toString());
    const QString metadata = stripHtml(index.data(UserDataTypes::METADATA).toString());

    int y = text.top();
    painter->setFont(titleFont);
    painter->setPen(main);
    painter->drawText(QRect(text.left(), y, text.width(), titleFm.height()), Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine,
                      titleFm.elidedText(title, Qt::ElideRight, text.width()));
    y += titleFm.height() + 2;

    painter->setFont(opt.font);
    painter->setPen(muted);
    painter->drawText(QRect(text.left(), y, text.width(), textFm.height()), Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine,
                      textFm.elidedText(description, Qt::ElideRight, text.width()));
    y += textFm.height() + 2;

    if (!metadata.isEmpty()) {
        painter->setFont(metaFont);
        painter->drawText(QRect(text.left(), y, text.width(), metaFm.height()), Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine,
                          metaFm.elidedText(metadata, Qt::ElideRight, text.width()));
    }

    painter->restore();
}
