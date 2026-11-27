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

#include <QAbstractItemModel>
#include <QList>
#include <QStandardItemModel>
#include <QTimer>
#include <QWidget>

class QLineEdit;
class QListView;

/**
 * The "Search all modpacks" box of NewInstanceDialog: it merges the results of several
 * modpack sources (Modrinth, CurseForge) into one dropdown and shows the source of every
 * result. It does not run the searches itself: searchRequested() is emitted for the host to
 * search the pages, and the box simply watches the pages' models and merges whatever appears.
 */
class ModpackSearchBox : public QWidget {
    Q_OBJECT
   public:
    explicit ModpackSearchBox(QWidget* parent = nullptr);

    /**
     * Merges the rows of @param model into the dropdown. @param id identifies the source in
     * resultActivated() (e.g. "modrinth"); @param displayName is what is shown on each result.
     */
    void addSource(const QString& id, const QString& displayName, QAbstractItemModel* model);

   signals:
    void searchRequested(const QString& term);
    void resultActivated(const QString& sourceId, const QModelIndex& sourceIndex);

   protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

   private:
    struct Source {
        QString id;
        QString displayName;
        QAbstractItemModel* model = nullptr;
    };

    void scheduleSearch();
    void startSearch();
    void rebuildResults();
    void showPopup();
    void hidePopup();
    void resizePopup();
    void activateRow(const QModelIndex& row);

    QLineEdit* m_box = nullptr;
    QListView* m_popup = nullptr;
    QStandardItemModel* m_resultsModel = nullptr;
    QTimer m_timer;
    QList<Source> m_sources;
    bool m_searching = false;
};
