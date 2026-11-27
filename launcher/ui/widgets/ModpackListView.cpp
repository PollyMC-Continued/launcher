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

#include "ModpackListView.h"

#include <QAbstractItemModel>
#include <QPainter>
#include <QPaintEvent>

ModpackListView::ModpackListView(QWidget* parent) : QListView(parent)
{
    setMouseTracking(true);  // hover highlight of the rows
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

void ModpackListView::setModel(QAbstractItemModel* model)
{
    if (auto* previous = QListView::model()) {
        disconnect(previous, &QAbstractItemModel::modelReset, this, &ModpackListView::refreshEmptyState);
        disconnect(previous, &QAbstractItemModel::rowsInserted, this, &ModpackListView::refreshEmptyState);
        disconnect(previous, &QAbstractItemModel::rowsRemoved, this, &ModpackListView::refreshEmptyState);
    }
    QListView::setModel(model);
    if (model) {
        connect(model, &QAbstractItemModel::modelReset, this, &ModpackListView::refreshEmptyState);
        connect(model, &QAbstractItemModel::rowsInserted, this, &ModpackListView::refreshEmptyState);
        connect(model, &QAbstractItemModel::rowsRemoved, this, &ModpackListView::refreshEmptyState);
    }
    refreshEmptyState();
}

void ModpackListView::setEmptyText(const QString& text)
{
    m_emptyText = text;
    refreshEmptyState();
}

void ModpackListView::setLoading(bool loading)
{
    m_loading = loading;
    refreshEmptyState();
}

void ModpackListView::refreshEmptyState()
{
    if (model() && model()->rowCount() > 0)
        m_loading = false;
    update();
}

void ModpackListView::paintEvent(QPaintEvent* event)
{
    QListView::paintEvent(event);

    if (model() && model()->rowCount() == 0) {
        const QString text = m_loading ? tr("Loading…") : m_emptyText;
        if (!text.isEmpty()) {
            QPainter painter(viewport());
            painter.setPen(palette().color(QPalette::PlaceholderText));
            painter.drawText(viewport()->rect(), Qt::AlignCenter | Qt::TextWordWrap, text);
        }
    }
}
