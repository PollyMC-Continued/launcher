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

#include <QStyledItemDelegate>

/**
 * Delegate for modpack result rows: the pack icon (or a coloured tile with the pack's initial
 * while there is no image), the pack name, a one-line description and - when the model
 * provides it - a small metadata line (downloads, loader, Minecraft version) underneath.
 * Expects the custom roles of ui/widgets/ProjectItem.h.
 */
class ModpackItemDelegate : public QStyledItemDelegate {
   public:
    explicit ModpackItemDelegate(QObject* parent = nullptr);

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};

/** Removes HTML tags and entities, so raw HTML from descriptions can be shown as plain text. */
QString stripHtml(QString text);
