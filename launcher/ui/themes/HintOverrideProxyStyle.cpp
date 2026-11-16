// SPDX-License-Identifier: GPL-3.0-only
/*
 *  PollyMC-Continued - Minecraft Launcher
 *  Copyright (C) 2024 TheKodeToad <TheKodeToad@proton.me>
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
 */

#include "HintOverrideProxyStyle.h"

#include <QApplication>
#include <QAbstractItemView>
#include <QDebug>
#include <QGuiApplication>
#include <QIcon>
#include <QImage>
#include <QPainter>
#include <QStyleFactory>
#include <QStyleHints>
#include <QStyleOptionViewItem>

#include <array>

#include "ui/themes/ThemeManager.h"

HintOverrideProxyStyle::HintOverrideProxyStyle(QStyle* style) : QProxyStyle(style)
{
    setObjectName(baseStyle()->objectName());
}

void HintOverrideProxyStyle::installFor(const QString& baseStyleName)
{
    // Re-installing the same style is pointless and dangerous: QApplication::setStyle()
    // deletes the previous style immediately, and this can be reached from inside a
    // widget's signal (e.g. a settings combo box whose popup is still being torn down).
    if (auto* current = qobject_cast<HintOverrideProxyStyle*>(QApplication::style())) {
        if (current->objectName().compare(baseStyleName, Qt::CaseInsensitive) == 0)
            return;
    }

    QStyle* base = QStyleFactory::create(baseStyleName);
    if (!base) {
        qWarning() << "Widget style" << baseStyleName << "is not available, falling back to Fusion";
        base = QStyleFactory::create("fusion");
    }
    if (!base)
        return;
    QApplication::setStyle(new HintOverrideProxyStyle(base));
}

namespace {
// Primitives QWindows11Style draws with Segoe Fluent Icons glyphs.
// Without that font they render as .notdef boxes (Win10, Wine,
// Winlator) — these alone fall back to Fusion rendering.
bool usesFluentGlyph(QStyle::PrimitiveElement pe)
{
    switch (pe) {
        case QStyle::PE_IndicatorSpinUp:
        case QStyle::PE_IndicatorSpinDown:
        case QStyle::PE_IndicatorArrowUp:
        case QStyle::PE_IndicatorArrowDown:
        case QStyle::PE_IndicatorArrowLeft:
        case QStyle::PE_IndicatorArrowRight:
        case QStyle::PE_IndicatorCheckBox:
        case QStyle::PE_IndicatorRadioButton:
        case QStyle::PE_IndicatorBranch:
        case QStyle::PE_IndicatorItemViewItemCheck:
            return true;
        default:
            return false;
    }
}

// One lazily-created Fusion style shared by both intercepts below.
// Function-local static: thread-safe, created once, lives forever.
QStyle* fluentFallbackStyle()
{
    static QStyle* fallback = QStyleFactory::create("fusion");
    return fallback;
}
}  // namespace

void HintOverrideProxyStyle::drawPrimitive(PrimitiveElement element,
                                           const QStyleOption* option,
                                           QPainter* painter,
                                           const QWidget* widget) const
{
    if (!ThemeManager::windows11StyleIsUsable() && usesFluentGlyph(element)) {
        if (QStyle* fallback = fluentFallbackStyle()) {
            fallback->drawPrimitive(element, option, painter, widget);
            return;
        }
    }
    QProxyStyle::drawPrimitive(element, option, painter, widget);
}

namespace {
// Perceived brightness (0-255) of a colour.
int brightness(const QColor& c)
{
    return (c.red() * 299 + c.green() * 587 + c.blue() * 114) / 1000;
}

// Everything that can change the colour the style ends up painting behind the text of a
// selected row: the palette colours the style derives its fills from (on Windows the
// highlight follows the system accent colour), the system colour scheme (light/dark/high
// contrast select different highlight fills), the view's alternating-row setting, the
// row's state, the device pixel ratio (fractional scaling rasterises differently) and
// the probe size.
struct ProbeKey {
    QRgb highlight = 0;
    QRgb base = 0;
    int colorScheme = 0;
    int dprMilli = 0;
    int width = 0;
    int height = 0;
    bool alternating = false;
    bool enabled = false;
    bool active = false;
    bool hovered = false;
    bool pressed = false;

    bool operator==(const ProbeKey& other) const
    {
        return highlight == other.highlight && base == other.base && colorScheme == other.colorScheme && dprMilli == other.dprMilli &&
               width == other.width && height == other.height && alternating == other.alternating && enabled == other.enabled &&
               active == other.active && hovered == other.hovered && pressed == other.pressed;
    }
};

// Probing is not free (a row is painted through the real style), but the result only
// depends on the key above, so a handful of entries covers every list, tree and table
// in the window. Fixed size, round-robin replacement, no allocations after warm-up.
struct ProbeCache {
    struct Entry {
        ProbeKey key;
        QColor color;
        bool used = false;
    };
    std::array<Entry, 8> entries;
    std::size_t next = 0;
};
}  // namespace

// The Windows 11 style does not always fill selected rows with palette().highlight()
// (it can be a light grey or an accent colour), so the palette cannot be trusted to
// pick a readable text colour. Instead, paint the row (without text) into an image
// using the real style and look at the colour that actually ended up behind the text.
//
// This used to allocate and paint a full-row image for every selected row on every
// repaint. On big rows that is megabytes per row per frame; when the allocation fails
// the painter cannot start ("Paint device returned engine == 0, type: 3") and the
// repeated churn under memory pressure took the launcher down with it. The probe is
// now clamped to a small window around the middle of the row (the fill is uniform, so
// the sampled colour is the same), rendered at the device pixel ratio, guarded against
// failed allocations, and cached.
QColor HintOverrideProxyStyle::readableTextColorFor(const QStyleOptionViewItem& item, const QWidget* widget) const
{
    const QColor paletteBased = brightness(item.palette.color(QPalette::Highlight)) < 140 ? QColor(Qt::white) : QColor(Qt::black);

    const QRect rect = item.rect;
    if (rect.width() < 4 || rect.height() < 4 || rect.width() > (1 << 20) || rect.height() > (1 << 20))
        return paletteBased;

    // The selection fill is uniform across the row, so a small window centred on the
    // middle of the row is enough - no need to render the whole (possibly very wide) row.
    constexpr int MaxProbeExtent = 128;
    const QSize probeSize(qMin(rect.width(), MaxProbeExtent), qMin(rect.height(), MaxProbeExtent));

    // devicePixelRatioF() is at least 1 for real widgets; clamp so a broken value can never
    // end up in the painter's coordinate transform
    const qreal deviceRatio = qMax<qreal>(1.0, widget ? widget->devicePixelRatioF() : 1.0);
    const int dprMilli = qRound(deviceRatio * 1000.0);

    bool alternating = false;
    if (const auto* view = qobject_cast<const QAbstractItemView*>(widget))
        alternating = view->alternatingRowColors();

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    const int colorScheme = int(QGuiApplication::styleHints()->colorScheme());
#else
    // Qt 6.4 has no colorScheme(); a palette change (already part of the key) covers the
    // common light/dark switches.
    const int colorScheme = 0;
#endif

    const ProbeKey key{ item.palette.color(QPalette::Highlight).rgba(),
                        item.palette.color(QPalette::Base).rgba(),
                        colorScheme,
                        dprMilli,
                        probeSize.width(),
                        probeSize.height(),
                        alternating,
                        item.state.testFlag(QStyle::State_Enabled),
                        item.state.testFlag(QStyle::State_Active),
                        item.state.testFlag(QStyle::State_MouseOver),
                        item.state.testFlag(QStyle::State_Sunken) };

    // Only ever called from drawControl, i.e. on the GUI thread - no locking needed.
    static ProbeCache cache;
    for (const auto& entry : cache.entries) {
        if (entry.used && entry.key == key)
            return entry.color;
    }

    QColor probed = paletteBased;  // keeps this safe even if the probe cannot run
    bool probeSucceeded = false;
    {
        const QSize imageSize(qMax(1, qRound(probeSize.width() * deviceRatio)), qMax(1, qRound(probeSize.height() * deviceRatio)));
        QImage image(imageSize, QImage::Format_ARGB32_Premultiplied);
        if (!image.isNull()) {  // a failed allocation must not be painted on
            image.setDevicePixelRatio(deviceRatio);
            image.fill(item.palette.color(QPalette::Base));
            QStyleOptionViewItem probe(item);
            probe.rect = QRect(QPoint(0, 0), probeSize);
            probe.text.clear();
            probe.icon = QIcon();
            probe.features &= ~(QStyleOptionViewItem::HasDisplay | QStyleOptionViewItem::HasDecoration | QStyleOptionViewItem::HasCheckIndicator);
            QPainter probePainter(&image);
            if (probePainter.isActive()) {  // the style must not paint on an inactive painter
                QProxyStyle::drawControl(CE_ItemViewItem, &probe, &probePainter, widget);
                probePainter.end();  // flush before reading pixels

                // sample the middle of the row, where the text will sit
                const QColor behind = image.pixelColor(image.width() / 2, image.height() / 2);
                if (behind.isValid()) {
                    probed = brightness(behind) < 140 ? QColor(Qt::white) : QColor(Qt::black);
                    probeSucceeded = true;
                }
            }
        }
    }

    // a probe that could not run (failed allocation, inactive painter) is retried next time
    // instead of pinning the fallback colour in the cache
    if (!probeSucceeded)
        return probed;

    auto& slot = cache.entries[cache.next];
    slot.key = key;
    slot.color = probed;
    slot.used = true;
    cache.next = (cache.next + 1) % cache.entries.size();
    return probed;
}

void HintOverrideProxyStyle::drawControl(ControlElement element,
                                         const QStyleOption* option,
                                         QPainter* painter,
                                         const QWidget* widget) const
{
    // The Windows 11 style paints selected item view rows (lists, trees, tables) with the
    // normal text colour on top of the blue highlight, which is unreadable. Pick a text
    // colour that contrasts with the actual highlight colour; unselected rows are untouched.
    if (element == CE_ItemViewItem && option && (option->state & State_Selected) &&
        objectName().compare(QStringLiteral("windows11"), Qt::CaseInsensitive) == 0) {
        if (const auto* item = qstyleoption_cast<const QStyleOptionViewItem*>(option)) {
            QStyleOptionViewItem adjusted(*item);
            const QColor textColor = readableTextColorFor(*item, widget);
            adjusted.palette.setColor(QPalette::Text, textColor);
            adjusted.palette.setColor(QPalette::HighlightedText, textColor);
            adjusted.palette.setColor(QPalette::WindowText, textColor);
            QProxyStyle::drawControl(element, &adjusted, painter, widget);
            return;
        }
    }
    QProxyStyle::drawControl(element, option, painter, widget);
}

void HintOverrideProxyStyle::drawComplexControl(ComplexControl control,
                                                const QStyleOptionComplex* option,
                                                QPainter* painter,
                                                const QWidget* widget) const
{
    if (!ThemeManager::windows11StyleIsUsable() && (control == CC_SpinBox || control == CC_ComboBox)) {
        if (QStyle* fallback = fluentFallbackStyle()) {
            fallback->drawComplexControl(control, option, painter, widget);
            return;
        }
    }
    QProxyStyle::drawComplexControl(control, option, painter, widget);
}

int HintOverrideProxyStyle::styleHint(QStyle::StyleHint hint,
                                      const QStyleOption* option,
                                      const QWidget* widget,
                                      QStyleHintReturn* returnData) const
{
    if (hint == QStyle::SH_ItemView_ActivateItemOnSingleClick)
        return 0;

    if (hint == QStyle::SH_Slider_AbsoluteSetButtons)
        return Qt::LeftButton | Qt::MiddleButton;

    if (hint == QStyle::SH_Slider_PageSetButtons)
        return Qt::RightButton;

    return QProxyStyle::styleHint(hint, option, widget, returnData);
}
