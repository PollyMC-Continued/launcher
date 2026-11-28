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

#include <QFocusEvent>
#include <QFontMetrics>
#include <QPainter>
#include <QPolygonF>
#include <QString>
#include <QStyle>
#include <QToolButton>

/**
 * The group picker of the New Instance dialog header: a pill-shaped button with a folder
 * icon, the group name and a dropdown chevron. It stays a QToolButton, so the existing
 * menu (InstantPopup) keeps working - only the painting is custom, with palette colours
 * only, like PillButton and DialogButton.
 */
class GroupPillButton : public QToolButton {
   public:
    explicit GroupPillButton(QWidget* parent = nullptr) : QToolButton(parent)
    {
        setAttribute(Qt::WA_Hover);
        setCursor(Qt::PointingHandCursor);
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    }

    QSize sizeHint() const override
    {
        const QString plain = text();
        const int textWidth = plain.isEmpty() ? 0 : QFontMetrics(font()).horizontalAdvance(plain);
        return QSize(IconSize + Gap + textWidth + Gap + ChevronWidth + 2 * HorizontalPadding, Height);
    }
    QSize minimumSizeHint() const override { return sizeHint(); }

   protected:
    void focusInEvent(QFocusEvent* event) override
    {
        m_keyboardFocus = event->reason() == Qt::TabFocusReason || event->reason() == Qt::BacktabFocusReason ||
                          event->reason() == Qt::ShortcutFocusReason;
        QToolButton::focusInEvent(event);
    }
    void focusOutEvent(QFocusEvent* event) override
    {
        m_keyboardFocus = false;
        QToolButton::focusOutEvent(event);
    }

    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const QPalette& pal = palette();
        const bool enabled = isEnabled();
        const bool hovered = enabled && underMouse();
        const bool pressed = enabled && isDown();

        const QRectF body = QRectF(rect()).adjusted(1, 1, -1, -1);
        QColor textColor = pal.color(QPalette::Active, QPalette::WindowText);
        QColor border = textColor;
        border.setAlpha(hovered || pressed ? 120 : 60);
        QColor fill = Qt::transparent;
        if (hovered || pressed) {
            fill = textColor;
            fill.setAlpha(pressed ? 40 : 22);
        }

        if (!enabled) {
            border.setAlpha(border.alpha() / 2);
            fill.setAlpha(fill.alpha() / 2);
            textColor.setAlpha(110);
        }

        painter.setPen(QPen(border, 1));
        painter.setBrush(fill);
        painter.drawRoundedRect(body, Height / 2.0, Height / 2.0);  // pill-shaped

        // the folder icon - the style's standard one, so it follows the theme
        int x = int(body.left()) + HorizontalPadding;
        if (const QIcon folder = style()->standardIcon(QStyle::SP_DirIcon); !folder.isNull()) {
            folder.paint(&painter, QRect(x, int(body.top()) + (int(body.height()) - IconSize) / 2, IconSize, IconSize));
        }
        x += IconSize + Gap;

        // the group name, elided to the room left beside the chevron
        const int chevronLeft = int(body.right()) - HorizontalPadding - ChevronWidth;
        painter.setFont(font());
        painter.setPen(textColor);
        if (!text().isEmpty()) {
            const QRect textRect(x, int(body.top()), chevronLeft - Gap - x, int(body.height()));
            painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine,
                             QFontMetrics(font()).elidedText(text(), Qt::ElideRight, textRect.width()));
        }

        // the dropdown chevron
        QColor chevronColor = pal.color(QPalette::Active, QPalette::WindowText);
        chevronColor.setAlpha(enabled ? 160 : 80);
        painter.setPen(Qt::NoPen);
        painter.setBrush(chevronColor);
        const qreal cx = chevronLeft + ChevronWidth / 2.0;
        const qreal cy = body.center().y();
        QPolygonF chevron;
        chevron << QPointF(cx - 4, cy - 2) << QPointF(cx + 4, cy - 2) << QPointF(cx, cy + 3);
        painter.drawPolygon(chevron);

        if (m_keyboardFocus) {
            QColor ring = pal.color(QPalette::Active, QPalette::Highlight);
            painter.setPen(QPen(ring, 1.5));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(body.adjusted(2, 2, -2, -2), Height / 2.0 - 2, Height / 2.0 - 2);
        }
    }

   private:
    static constexpr int Height = 34;          // like the dialog's action buttons
    static constexpr int IconSize = 16;         // the folder icon
    static constexpr int Gap = 8;               // icon <-> text <-> chevron
    static constexpr int ChevronWidth = 12;     // room reserved for the chevron
    static constexpr int HorizontalPadding = 14;

    bool m_keyboardFocus = false;
};
