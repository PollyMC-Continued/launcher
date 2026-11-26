// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QFont>
#include <QLabel>
#include <QPalette>
#include <QStyle>
#include <QWidget>

// Styles the small explanation lines shown under a setting (labels flagged with the dynamic property
// "settingDescription" in the page's .ui file): muted colour and slightly smaller. Under a radio button the
// line is also indented ("settingDescriptionIndent") so it lines up with the radio button's label; the
// on/off switches have their label at the left edge, so their descriptions are not indented.
inline void styleSettingDescriptions(QWidget* page)
{
    const QStyle* style = page->style();
    const int indent = style->pixelMetric(QStyle::PM_ExclusiveIndicatorWidth) + style->pixelMetric(QStyle::PM_RadioButtonLabelSpacing);
    for (auto* label : page->findChildren<QLabel*>()) {
        if (!label->property("settingDescription").toBool())
            continue;
        label->setForegroundRole(QPalette::PlaceholderText);
        label->setContentsMargins(label->property("settingDescriptionIndent").toBool() ? indent : 0, 0, 0, 0);
        QFont font = label->font();
        if (font.pointSizeF() > 1)
            font.setPointSizeF(font.pointSizeF() - 1);
        label->setFont(font);
    }
}
