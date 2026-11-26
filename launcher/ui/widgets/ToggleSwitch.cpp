// SPDX-License-Identifier: GPL-3.0-only

#include "ToggleSwitch.h"

#include <QFontMetrics>
#include <QPainter>
#include <QPropertyAnimation>
#include <QStyle>
#include <QStyleOption>

namespace {
constexpr int TrackWidth = 38;
constexpr int TrackHeight = 22;
constexpr int KnobMargin = 3;
constexpr int TextGap = 14;
constexpr int EdgeMargin = 2;

QColor mix(const QColor& from, const QColor& to, qreal t)
{
    return QColor::fromRgbF(from.redF() + (to.redF() - from.redF()) * t, from.greenF() + (to.greenF() - from.greenF()) * t,
                            from.blueF() + (to.blueF() - from.blueF()) * t, from.alphaF() + (to.alphaF() - from.alphaF()) * t);
}
}  // namespace

ToggleSwitch::ToggleSwitch(QWidget* parent) : QCheckBox(parent)
{
    init();
}

ToggleSwitch::ToggleSwitch(const QString& text, QWidget* parent) : QCheckBox(text, parent)
{
    init();
}

void ToggleSwitch::init()
{
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    m_knobPosition = isChecked() ? 1.0 : 0.0;

    m_animation = new QPropertyAnimation(this, "knobPosition", this);
    m_animation->setDuration(140);
    m_animation->setEasingCurve(QEasingCurve::OutCubic);
    connect(this, &QAbstractButton::toggled, this, &ToggleSwitch::animateTo);
}

QString ToggleSwitch::plainText() const
{
    // "&Name" -> "Name", "Fish && Chips" -> "Fish & Chips" (the mnemonic still works, it is just not drawn)
    QString label = text();
    label.replace(QStringLiteral("&&"), QStringLiteral("\x01"));
    label.remove(QLatin1Char('&'));
    label.replace(QLatin1Char('\x01'), QLatin1Char('&'));
    return label;
}

void ToggleSwitch::animateTo(bool checked)
{
    const qreal end = checked ? 1.0 : 0.0;
    m_animation->stop();
    if (!isVisible()) {
        // settings being loaded into a page that is not shown yet: no animation, just the right state
        setKnobPosition(end);
        return;
    }
    m_animation->setStartValue(m_knobPosition);
    m_animation->setEndValue(end);
    m_animation->start();
}

void ToggleSwitch::setKnobPosition(qreal position)
{
    m_knobPosition = qBound<qreal>(0.0, position, 1.0);
    update();
}

QSize ToggleSwitch::sizeHint() const
{
    const QFontMetrics fm(font());
    return QSize(fm.horizontalAdvance(plainText()) + TextGap + TrackWidth + EdgeMargin * 2, qMax(fm.height() + 10, TrackHeight + 8));
}

QSize ToggleSwitch::minimumSizeHint() const
{
    // long labels may be elided instead of forcing the whole settings window to become wider
    const QFontMetrics fm(font());
    return QSize(qMin(fm.horizontalAdvance(plainText()), 160) + TextGap + TrackWidth + EdgeMargin * 2, sizeHint().height());
}

bool ToggleSwitch::hitButton(const QPoint& pos) const
{
    return rect().contains(pos);
}

void ToggleSwitch::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // lets a style sheet (e.g. the search highlight flash) paint its background
    QStyleOption option;
    option.initFrom(this);
    style()->drawPrimitive(QStyle::PE_Widget, &option, &p, this);

    if (!isEnabled())
        p.setOpacity(0.45);

    const QPalette pal = palette();

    // label
    const QRect textRect(0, 0, qMax(0, width() - TrackWidth - TextGap - EdgeMargin * 2), height());
    p.setPen(pal.color(QPalette::Text));
    p.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine,
               QFontMetrics(font()).elidedText(plainText(), Qt::ElideRight, textRect.width()));

    // track
    const QRect track(width() - TrackWidth - EdgeMargin, (height() - TrackHeight) / 2, TrackWidth, TrackHeight);
    QColor off = pal.color(QPalette::Text);
    off.setAlphaF(0.28);
    const QColor on = pal.color(QPalette::Highlight);
    p.setPen(Qt::NoPen);
    p.setBrush(mix(off, on, m_knobPosition));
    p.drawRoundedRect(track, TrackHeight / 2.0, TrackHeight / 2.0);

    // knob
    const int knobSize = TrackHeight - KnobMargin * 2;
    const qreal left = track.left() + KnobMargin;
    const qreal right = track.right() + 1 - KnobMargin - knobSize;
    p.setBrush(QColor(255, 255, 255));
    p.drawEllipse(QRectF(left + (right - left) * m_knobPosition, track.top() + KnobMargin, knobSize, knobSize));

    // keyboard focus
    if (hasFocus()) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(on, 2));
        p.drawRoundedRect(QRectF(track).adjusted(-2, -2, 2, 2), TrackHeight / 2.0 + 2, TrackHeight / 2.0 + 2);
    }
}
