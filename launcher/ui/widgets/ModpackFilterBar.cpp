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

#include <QPair>
#include <QVBoxLayout>

#include "ui/widgets/FlowLayout.h"
#include "ui/widgets/ModFilterWidget.h"
#include "ui/widgets/PillButton.h"

ModpackFilterBar::ModpackFilterBar(ModFilterWidget* filterWidget, QWidget* parent) : QWidget(parent), m_filterWidget(filterWidget)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    auto* layout = new FlowLayout(this, 0, 6, 4);

    const QList<QPair<ModPlatform::ModLoaderType, QString>> loaders = {
        { ModPlatform::ModLoaderType::NeoForge, tr("NeoForge") }, { ModPlatform::ModLoaderType::Forge, tr("Forge") },
        { ModPlatform::ModLoaderType::Fabric, tr("Fabric") },    { ModPlatform::ModLoaderType::Quilt, tr("Quilt") },
        { ModPlatform::ModLoaderType::LiteLoader, tr("LiteLoader") },
    };
    for (const auto& loader : loaders) {
        auto* pill = new PillButton(loader.second, this);
        pill->setCheckable(true);
        layout->addWidget(pill);
        m_loaderPills.insert(int(loader.first), pill);
        connect(pill, &PillButton::toggled, this, [this, loader](bool checked) {
            m_filterWidget->setLoaderChecked(loader.first, checked);
        });
    }

    m_moreButton = new PillButton(tr("More filters…"), this);
    layout->addWidget(m_moreButton);
    connect(m_moreButton, &PillButton::clicked, this, &ModpackFilterBar::showMoreFilters);

    // the full filter widget lives in a popup below "More filters…" - it keeps every existing
    // filter (versions, categories, open source, side, extended loaders)
    m_popup = new QWidget(this, Qt::Popup);
    auto* popupLayout = new QVBoxLayout(m_popup);
    popupLayout->setContentsMargins(8, 8, 8, 8);
    m_filterWidget->setParent(m_popup);
    m_filterWidget->show();
    popupLayout->addWidget(m_filterWidget);
    m_popup->setMinimumWidth(420);

    // the popup can change the same filters, so keep the chips in sync
    connect(m_filterWidget, &ModFilterWidget::filterChanged, this, &ModpackFilterBar::syncPills);
    syncPills();
}

void ModpackFilterBar::syncPills()
{
    const auto& filter = m_filterWidget->currentFilter();
    for (auto it = m_loaderPills.constBegin(); it != m_loaderPills.constEnd(); ++it) {
        const bool checked = filter.loaders.testFlag(ModPlatform::ModLoaderType(it.key()));
        it.value()->setChecked(checked);
    }
}

void ModpackFilterBar::showMoreFilters()
{
    m_popup->move(m_moreButton->mapToGlobal(QPoint(0, m_moreButton->height() + 2)));
    m_popup->show();
    m_popup->activateWindow();
}
