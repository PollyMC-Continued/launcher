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

#include <QListView>

/**
 * A QListView for modpack result lists. Draws a placeholder text while the model has no rows:
 * "Loading…" while a request is running, the page's empty text (e.g. "No modpacks found")
 * afterwards. Rows are meant to be drawn by ModpackItemDelegate.
 */
class ModpackListView : public QListView {
    Q_OBJECT
   public:
    explicit ModpackListView(QWidget* parent = nullptr);

    void setModel(QAbstractItemModel* model) override;

    /** Text painted when the model has no rows and nothing is loading. */
    void setEmptyText(const QString& text);
    /** Shows the loading placeholder instead of the empty text. Cleared automatically when rows appear. */
    void setLoading(bool loading);

   protected:
    void paintEvent(QPaintEvent* event) override;

   private:
    void refreshEmptyState();

    QString m_emptyText;
    bool m_loading = false;
};
