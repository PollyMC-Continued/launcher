// SPDX-License-Identifier: GPL-3.0-only
/*
 *  PollyMC-Continued - Minecraft Launcher
 *  Copyright (c) 2026 PollyMC-Continued Contributors
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

#include "ToastNotification.h"

#include <QAbstractAnimation>
#include <QApplication>
#include <QEvent>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QSizePolicy>
#include <QTimer>
#include <QVBoxLayout>
#include <QEasingCurve>

namespace {
constexpr int kMargin = 12;
constexpr int kAnimationDurationMs = 250;
constexpr int kSlideOffset = 24;
}  // namespace

ToastNotification::ToastNotification(QWidget* parent) : QFrame(parent)
{
    setObjectName(QStringLiteral("ToastNotification"));
    setFocusPolicy(Qt::NoFocus);
    hide();

    applyStyle();

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(24);
    shadow->setOffset(0, 3);
    shadow->setColor(QColor(0, 0, 0, 90));
    setGraphicsEffect(shadow);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(kMargin, kMargin, kMargin, kMargin);
    layout->setSpacing(6);

    auto* headerRow = new QHBoxLayout();
    headerRow->setSpacing(6);

    // A plain star glyph instead of the colour emoji: many Linux setups have no emoji font, which
    // left an empty gap that pushed the title around.
    m_iconLabel = new QLabel(QStringLiteral("★"), this);
    m_iconLabel->setObjectName(QStringLiteral("iconLabel"));
    m_iconLabel->setFocusPolicy(Qt::NoFocus);
    headerRow->addWidget(m_iconLabel, 0, Qt::AlignTop);

    m_titleLabel = new QLabel(this);
    m_titleLabel->setFocusPolicy(Qt::NoFocus);
    QFont titleFont = m_titleLabel->font();
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    // Wrap instead of clipping when the title is long (translations, larger fonts, high DPI).
    m_titleLabel->setWordWrap(true);
    m_titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    headerRow->addWidget(m_titleLabel, 1);

    m_closeButton = new QPushButton(QStringLiteral("×"), this);
    m_closeButton->setObjectName(QStringLiteral("closeButton"));
    m_closeButton->setToolTip(tr("Dismiss"));
    m_closeButton->setFocusPolicy(Qt::NoFocus);
    m_closeButton->setFixedSize(24, 24);
    m_closeButton->setCursor(Qt::PointingHandCursor);
    connect(m_closeButton, &QPushButton::clicked, this, &ToastNotification::onCloseClicked);
    headerRow->addWidget(m_closeButton, 0, Qt::AlignTop);

    layout->addLayout(headerRow);

    m_bodyLabel = new QLabel(this);
    m_bodyLabel->setFocusPolicy(Qt::NoFocus);
    m_bodyLabel->setWordWrap(true);
    layout->addWidget(m_bodyLabel);

    m_actionButton = new QPushButton(this);
    m_actionButton->setObjectName(QStringLiteral("actionButton"));
    m_actionButton->setCursor(Qt::PointingHandCursor);
    m_actionButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    connect(m_actionButton, &QPushButton::clicked, this, &ToastNotification::onActionClicked);
    layout->addWidget(m_actionButton, 0, Qt::AlignRight);

    m_autoHideTimer = new QTimer(this);
    m_autoHideTimer->setSingleShot(true);
    connect(m_autoHideTimer, &QTimer::timeout, this, [this]() { slideOut(false); });

    m_animation = new QPropertyAnimation(this, "pos", this);
    m_animation->setDuration(kAnimationDurationMs);
    m_animation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_animation, &QPropertyAnimation::finished, this, &ToastNotification::onAnimationFinished);
}

void ToastNotification::applyStyle()
{
    // Read colours from the parent/app palette rather than our own: once a
    // stylesheet is applied our own palette is rewritten by it, which would
    // otherwise feed back into itself.
    const QPalette palette = parentWidget() ? parentWidget()->palette() : QApplication::palette();
    const auto background = palette.color(QPalette::Window);
    const auto border = palette.color(QPalette::Mid);
    const auto accent = palette.color(QPalette::Highlight);
    const auto accentText = palette.color(QPalette::HighlightedText);
    const auto accentHover = accent.lighter(115);
    const auto text = palette.color(QPalette::WindowText);
    const QString style = QStringLiteral(
        "#ToastNotification {"
        "  background-color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 8px;"
        "}"
        "#ToastNotification QLabel {"
        "  color: %6;"
        "}"
        "#ToastNotification QLabel#iconLabel {"
        "  color: #e3b341;"
        "}"
        "#ToastNotification QPushButton {"
        "  border: none;"
        "  border-radius: 4px;"
        "  padding: 6px 14px;"
        "  color: %6;"
        "  background-color: transparent;"
        "}"
        "#ToastNotification QPushButton:hover {"
        "  background-color: %3;"
        "  color: %4;"
        "}"
        "#ToastNotification QPushButton#closeButton {"
        "  padding: 0px;"
        "  border-radius: 12px;"
        "}"
        "#ToastNotification QPushButton#actionButton {"
        "  background-color: %3;"
        "  color: %4;"
        "  font-weight: 600;"
        "}"
        "#ToastNotification QPushButton#actionButton:hover {"
        "  background-color: %5;"
        "}")
        .arg(background.name())
        .arg(border.name())
        .arg(accent.name())
        .arg(accentText.name())
        .arg(accentHover.name())
        .arg(text.name());

    if (m_applyingStyle || style == m_appliedStyle)
        return;
    m_applyingStyle = true;
    m_appliedStyle = style;
    setStyleSheet(style);
    m_applyingStyle = false;
}

void ToastNotification::changeEvent(QEvent* event)
{
    // Follow runtime light/dark (or theme) switches.
    if (event->type() == QEvent::ApplicationPaletteChange || event->type() == QEvent::PaletteChange)
        applyStyle();
    QFrame::changeEvent(event);
    // A font change alters how much room the text needs.
    if (event->type() == QEvent::FontChange || event->type() == QEvent::ApplicationFontChange)
        updateSize();
}

bool ToastNotification::event(QEvent* event)
{
    // Pause auto-hide while the pointer is over the card so it doesn't vanish mid-click.
    if (event->type() == QEvent::Enter) {
        m_hovered = true;
        if (m_visible)
            m_autoHideTimer->stop();
    } else if (event->type() == QEvent::Leave) {
        m_hovered = false;
        if (m_visible)
            m_autoHideTimer->start(m_displayMs);
    }
    return QFrame::event(event);
}

void ToastNotification::prepareToast(const QString& title,
                                     const QString& body,
                                     const QString& actionText,
                                     std::function<void()> action)
{
    m_action = std::move(action);
    m_titleLabel->setText(title);
    m_bodyLabel->setText(body);
    m_actionButton->setText(actionText);
    m_actionButton->setVisible(!actionText.isEmpty());
    updateSize();
}

void ToastNotification::updateSize()
{
    if (!layout())
        return;
    // The width follows the font (so large UI fonts / high DPI / long translations don't clip the
    // text) but never exceeds the parent. The height is then derived from the wrapped text, so the
    // card is always exactly as tall as its content.
    int w = qMax(340, fontMetrics().averageCharWidth() * 46);
    if (parentWidget())
        w = qMin(w, qMax(220, parentWidget()->width() - 2 * kMargin));
    layout()->activate();
    const int h = qMax(layout()->totalHeightForWidth(w), layout()->minimumSize().height());
    setFixedSize(w, h);
}

void ToastNotification::showToast(const QPoint& finalPos, int displayMs)
{
    if (m_visible || m_animation->state() != QAbstractAnimation::Stopped)
        return;

    m_finalPos = finalPos;
    m_displayMs = displayMs;

    move(m_finalPos.x(), m_finalPos.y() + kSlideOffset);
    show();
    raise();  // always sit above sibling widgets (status bar, view, ...)

    m_visible = true;
    if (!m_hovered)
        m_autoHideTimer->start(displayMs);
    m_animation->setStartValue(pos());
    m_animation->setEndValue(m_finalPos);
    m_animation->start();
}

void ToastNotification::updatePosition(const QPoint& finalPos)
{
    m_finalPos = finalPos;
    if (!m_visible)
        return;
    if (m_animation->state() == QAbstractAnimation::Stopped)
        move(m_finalPos);
    else  // retarget the in-flight slide-in so a resize mid-animation lands right
        m_animation->setEndValue(m_finalPos);
}

bool ToastNotification::isBusy() const
{
    return m_visible || m_animation->state() != QAbstractAnimation::Stopped;
}

void ToastNotification::dismissToast()
{
    slideOut(true);
}

void ToastNotification::onCloseClicked()
{
    slideOut(true);
}

void ToastNotification::onActionClicked()
{
    m_autoHideTimer->stop();
    if (m_action)
        m_action();
    slideOut(true);
}

void ToastNotification::onAnimationFinished()
{
    if (!m_visible)
        hide();
}

void ToastNotification::slideOut(bool emitDismissed)
{
    if (!m_visible)
        return;
    if (m_animation->state() != QAbstractAnimation::Stopped)
        m_animation->stop();  // cut the slide-in short, otherwise a click in the first 250ms is lost

    m_autoHideTimer->stop();
    m_visible = false;
    m_animation->setStartValue(pos());
    m_animation->setEndValue(QPoint(m_finalPos.x(), m_finalPos.y() + kSlideOffset));
    m_animation->start();

    if (emitDismissed)
        emit dismissed();
}
