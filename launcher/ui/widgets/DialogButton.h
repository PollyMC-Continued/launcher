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
#include <QPushButton>
#include <QString>
#include <QStyle>
#include <QStyleOptionButton>

enum class DialogButtonRole {
    Primary,    // OK / the action: filled with the accent (highlight) colour
    Secondary,  // Cancel: soft fill
    Ghost,      // Help: outline only
};

// A flat, rounded button that takes all of its colours from the palette (so it follows the
// theme). Originally a local class of PageDialog; moved here so NewInstanceDialog can use the
// same buttons.
class DialogButton : public QPushButton {
   public:
    DialogButton(const QString& text, DialogButtonRole role, QWidget* parent = nullptr) : QPushButton(text, parent), m_role(role)
    {
        setAttribute(Qt::WA_Hover);
        setCursor(Qt::PointingHandCursor);
        if (role == DialogButtonRole::Primary) {
            QFont f = font();
            f.setWeight(QFont::DemiBold);
            setFont(f);
        }
    }

    QSize sizeHint() const override
    {
        QString plain = text();
        plain.replace(QStringLiteral("&&"), QStringLiteral("\x01"));
        plain.remove(QLatin1Char('&'));
        plain.replace(QLatin1Char('\x01'), QLatin1Char('&'));
        // a fixed minimum width keeps "OK" and "Apply" the same size, so the row does not jump
        return QSize(qMax(MinWidth, QFontMetrics(font()).horizontalAdvance(plain) + 2 * HorizontalPadding), Height);
    }
    QSize minimumSizeHint() const override { return sizeHint(); }

   protected:
    void focusInEvent(QFocusEvent* event) override
    {
        m_keyboardFocus = event->reason() == Qt::TabFocusReason || event->reason() == Qt::BacktabFocusReason ||
                          event->reason() == Qt::ShortcutFocusReason;
        QPushButton::focusInEvent(event);
    }
    void focusOutEvent(QFocusEvent* event) override
    {
        m_keyboardFocus = false;
        QPushButton::focusOutEvent(event);
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
        QColor fill = Qt::transparent;
        QColor border = Qt::transparent;

        switch (m_role) {
            case DialogButtonRole::Primary: {
                fill = pal.color(QPalette::Active, QPalette::Highlight);
                // dark text on bright highlights (a green accent), white on dark ones - same rule as the sidebar
                textColor = fill.lightness() > 128 ? QColor("#191919") : QColor("#ffffff");
                if (pressed)
                    fill = fill.darker(115);
                else if (hovered)
                    fill = fill.lighter(112);
                break;
            }
            case DialogButtonRole::Secondary:
                fill = textColor;
                fill.setAlpha(pressed ? 56 : hovered ? 40 : 24);
                break;
            case DialogButtonRole::Ghost:
                border = textColor;
                border.setAlpha(hovered || pressed ? 120 : 60);
                if (hovered || pressed) {
                    fill = textColor;
                    fill.setAlpha(pressed ? 40 : 22);
                }
                break;
        }

        if (!enabled) {
            fill.setAlpha(fill.alpha() / 2);
            border.setAlpha(border.alpha() / 2);
            textColor.setAlpha(110);
        }

        painter.setPen(border.alpha() > 0 ? QPen(border, 1) : QPen(Qt::NoPen));
        painter.setBrush(fill);
        painter.drawRoundedRect(body, Radius, Radius);

        if (m_keyboardFocus) {
            QColor ring = m_role == DialogButtonRole::Primary ? textColor : pal.color(QPalette::Active, QPalette::Highlight);
            painter.setPen(QPen(ring, 1.5));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(body.adjusted(2, 2, -2, -2), Radius - 2, Radius - 2);
        }

        QStyleOptionButton opt;
        opt.initFrom(this);
        const int mnemonic = style()->styleHint(QStyle::SH_UnderlineShortcut, &opt, this) ? Qt::TextShowMnemonic : Qt::TextHideMnemonic;
        painter.setPen(textColor);
        painter.setFont(font());
        painter.drawText(rect(), Qt::AlignCenter | Qt::TextSingleLine | mnemonic, text());
    }

   private:
    static constexpr int Height = 34;
    static constexpr int MinWidth = 96;
    static constexpr int HorizontalPadding = 22;
    static constexpr qreal Radius = 8;  // matches the sidebar pills

    DialogButtonRole m_role;
    bool m_keyboardFocus = false;
};
