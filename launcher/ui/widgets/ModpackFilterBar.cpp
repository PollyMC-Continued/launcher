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

#include "ModpackFilterBar.h"

#include <QFont>
#include <QVBoxLayout>

#include "ui/widgets/FlowLayout.h"
#include "ui/widgets/ModFilterWidget.h"
#include "ui/widgets/PillButton.h"

namespace {
// The small filter chip: 26px tall with 12px text and a 13px radius - a real pill, but
// visibly smaller than the dialog's action buttons.
class FilterChip : public PillButton {
   public:
    explicit FilterChip(const QString& text, QWidget* parent = nullptr) : PillButton(text, parent)
    {
        m_height = 26;
        m_radius = 13;
        QFont chipFont = font();
        chipFont.setPixelSize(12);
        setFont(chipFont);
        setCursor(Qt::PointingHandCursor);
    }
};

QString loaderName(ModPlatform::ModLoaderType loader)
{
    switch (loader) {
        case ModPlatform::ModLoaderType::NeoForge:
            return QObject::tr("NeoForge");
        case ModPlatform::ModLoaderType::Forge:
            return QObject::tr("Forge");
        case ModPlatform::ModLoaderType::Fabric:
            return QObject::tr("Fabric");
        case ModPlatform::ModLoaderType::Quilt:
            return QObject::tr("Quilt");
        case ModPlatform::ModLoaderType::LiteLoader:
            return QObject::tr("LiteLoader");
        case ModPlatform::ModLoaderType::Babric:
            return QObject::tr("Babric");
        case ModPlatform::ModLoaderType::BTA:
            return QObject::tr("BTA");
        case ModPlatform::ModLoaderType::LegacyFabric:
            return QObject::tr("Legacy Fabric");
        case ModPlatform::ModLoaderType::Ornithe:
            return QObject::tr("Ornithe");
        case ModPlatform::ModLoaderType::Rift:
            return QObject::tr("Rift");
        default:
            return {};
    }
}
}  // namespace

ModpackFilterBar::ModpackFilterBar(ModFilterWidget* filterWidget, QWidget* parent) : QWidget(parent), m_filterWidget(filterWidget)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    m_layout = new FlowLayout(this, 0, 6, 4);

    m_moreButton = new FilterChip(tr("+ Filters"), this);
    m_moreButton->setToolTip(tr("Choose filters (versions, loaders, categories…)"));
    m_layout->addWidget(m_moreButton);
    connect(m_moreButton, &PillButton::clicked, this, &ModpackFilterBar::showMoreFilters);

    // the full filter widget lives in a popup below "+ Filters" - it keeps every existing
    // filter (versions, categories, open source, side, extended loaders)
    m_popup = new QWidget(this, Qt::Popup);
    auto* popupLayout = new QVBoxLayout(m_popup);
    popupLayout->setContentsMargins(8, 8, 8, 8);
    m_filterWidget->setParent(m_popup);
    m_filterWidget->show();
    popupLayout->addWidget(m_filterWidget);
    m_popup->setMinimumWidth(420);

    // the chips always mirror the active filters - only the popup sets them in bulk
    connect(m_filterWidget, &ModFilterWidget::filterChanged, this, &ModpackFilterBar::rebuildChips);
    rebuildChips();
}

void ModpackFilterBar::rebuildChips()
{
    // drop the old chips, but keep the "+ Filters" pill (it is re-added last below)
    QList<QWidget*> old;
    while (m_layout->count()) {
        QLayoutItem* item = m_layout->takeAt(0);
        if (item->widget())
            old.append(item->widget());
        delete item;
    }
    for (auto* widget : old) {
        if (widget != m_moreButton)
            widget->deleteLater();
    }

    const auto& filter = m_filterWidget->currentFilter();

    // Minecraft versions
    for (const auto& version : filter.versions) {
        const QString label = version.toString();
        addChip(label, [this, label] { m_filterWidget->clearVersion(label); });
    }

    // mod loaders
    for (auto loader : ModPlatform::modLoaderTypesToList(filter.loaders)) {
        const QString label = loaderName(loader);
        if (label.isEmpty())
            continue;
        addChip(label, [this, loader] { m_filterWidget->setLoaderChecked(loader, false); });
    }

    // categories
    for (const auto& category : m_filterWidget->checkedCategories()) {
        addChip(category.first, [this, id = category.second] { m_filterWidget->clearCategory(id); });
    }

    m_layout->addWidget(m_moreButton);
}

void ModpackFilterBar::addChip(const QString& label, const std::function<void()>& clear)
{
    auto* chip = new FilterChip(label, this);
    chip->setToolTip(tr("Remove this filter"));
    // clear() writes the change through to the filter widget, which emits filterChanged,
    // which rebuilds the chips - no manual refresh needed here
    connect(chip, &PillButton::clicked, this, clear);
    m_layout->addWidget(chip);
}

void ModpackFilterBar::showMoreFilters()
{
    m_popup->move(m_moreButton->mapToGlobal(QPoint(0, m_moreButton->height() + 2)));
    m_popup->show();
    m_popup->activateWindow();
}
