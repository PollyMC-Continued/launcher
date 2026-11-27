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

#include <QHash>
#include <QWidget>

#include "modplatform/ModIndex.h"

class ModFilterWidget;
class PillButton;

/**
 * Quick filter chips for the modpack browse pages: the five common mod loaders, plus a
 * "More filters…" chip that opens the full ModFilterWidget (versions, categories, open
 * source, ...) in a popup. Chip changes are written through to the ModFilterWidget, which
 * emits filterChanged exactly as before, so pages keep their existing search flow.
 */
class ModpackFilterBar : public QWidget {
    Q_OBJECT
   public:
    /**
     * @param filterWidget the page's ModFilterWidget; it is reparented into the popup opened
     *        by "More filters…". The page keeps ownership.
     */
    ModpackFilterBar(ModFilterWidget* filterWidget, QWidget* parent = nullptr);

   private:
    void syncPills();
    void showMoreFilters();

    ModFilterWidget* m_filterWidget = nullptr;
    PillButton* m_moreButton = nullptr;
    QHash<int, PillButton*> m_loaderPills;
    QWidget* m_popup = nullptr;
};
