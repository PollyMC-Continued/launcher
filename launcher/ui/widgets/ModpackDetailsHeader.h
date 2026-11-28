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

#include <QString>
#include <QWidget>

class ElidedLabel;
class QIcon;
class QLabel;

/**
 * The header at the top of a modpack page's details pane: a 48px pack tile (the pack's logo,
 * or a coloured tile with the pack's initial while there is no image), the pack name in
 * bold 16px text, and "by <author> · <platform>" in muted text.
 *
 * The version row and the description below it stay part of the page's own .ui - only this
 * header is shared, so every platform page gets the same look.
 */
class ModpackDetailsHeader : public QWidget {
    Q_OBJECT
   public:
    explicit ModpackDetailsHeader(QWidget* parent = nullptr);

    /** Shows the pack. An empty author omits the "by …" part of the muted line. */
    void setPack(const QString& name, const QString& author, const QString& platform);
    /** Updates the tile; call when the pack's logo finishes loading. */
    void setIcon(const QIcon& icon);
    /** The pack currently shown - used to ignore late logo callbacks after a selection change. */
    QString name() const { return m_name; }
    /** Resets to the empty look (no name, no logo, no byline). */
    void clearPack();

    /** Muted styling for the "Select a modpack" placeholder of the details panes. */
    static void stylePlaceholder(QLabel* label);

   private:
    class PackTile;

    PackTile* m_tile = nullptr;
    ElidedLabel* m_nameLabel = nullptr;
    QLabel* m_bylineLabel = nullptr;
    QString m_name;
};
