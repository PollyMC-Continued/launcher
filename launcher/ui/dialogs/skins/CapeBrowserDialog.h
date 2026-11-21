// SPDX-License-Identifier: GPL-3.0-only
/*
 *  PollyMC-Continued - Minecraft Launcher
 *  Copyright (C) 2026 PollyMC Continued Contributors
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

#pragma once

#include <memory>

#include <QDialog>
#include <QHash>
#include <QImage>
#include <QList>
#include <QSet>
#include <QStringList>

#include "minecraft/skins/CraftySkinAPI.h"
#include "minecraft/skins/SkinModel.h"
#include "ui/dialogs/skins/draw/SkinOpenGLWindow.h"

// 2:1 cape canvas the 3D preview expects (OptiFine capes are cropped to 46x22)
QImage normalizedCape(const QImage& cape);

class QCheckBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QTimer;

// Lets the user pick a cape: capes crafty.gg tracks for a player name, plus direct PNG URL
// imports. Laid out like the skin browser (grid on the left, 3D preview on the right), and
// the preview wears the picked cape on the skin that is currently selected in the skin
// manager. Built entirely in C++ (no .ui file); crafty.gg requests are rate-limited through
// Crafty::API and aborted when the dialog closes.
class CapeBrowserDialog : public QDialog, public SkinProvider {
    Q_OBJECT

   public:
    // currentSkin is the skin to dress up in the preview; it is copied, so it only has to
    // outlive the constructor. nullptr shows the default (empty) model.
    explicit CapeBrowserDialog(const SkinModel* currentSkin, QWidget* parent = nullptr);
    ~CapeBrowserDialog() override;

    SkinModel* getSelectedSkin() override { return m_skin.get(); }
    QHash<QString, QImage> capes() override;

   signals:
    // emitted by "Use cape" with the cape the user confirmed, together with the player it
    // was found for (empty for URL imports)
    void selected(const QString& player, const Crafty::CapePtr& cape);

   protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void accept() override;
    void reject() override;

   private slots:
    void search();
    void importUrl();
    void useCape();
    void currentRowChanged(int row);
    void previousPage();
    void nextPage();

   private:
    void setLoading(bool loading);
    void fillList();
    void updateDetails();
    void showError(const QString& reason);
    QString displayName(const Crafty::CapePtr& cape) const;
    QPixmap thumbnail(const Crafty::CapePtr& cape) const;
    // the grid fills itself the way the skin browser does: capes are looked up for the players of the
    // skin catalog, one player at a time, and show up as they arrive. A page of the pager is one
    // batch of players; pages that were already loaded are kept, so going back is instant
    void discover(int apiPage);
    void lookupNext(int request, const QStringList& names, int index);
    void finishDiscovery(const QString& error);
    // force: keep the cape even if it does not match the cape-name search (a player's own capes)
    bool addCape(const Crafty::CapePtr& cape, bool force = false);
    bool matchesFilter(const Crafty::CapePtr& cape) const;
    void appendItem(const Crafty::CapePtr& cape, int row);
    void showPage(int page);
    void updatePager();
    void setupFallbackSkin();

    // grey shimmering placeholders at the end of the grid while capes are being fetched
    void startSkeleton(int count);
    void stopSkeleton();
    void tickSkeleton();
    QPixmap skeletonPixmap(int slot) const;

   private:
    Crafty::API* m_api;
    QList<Crafty::CapePtr> m_capes;
    Crafty::CapePtr m_current;
    QString m_player;
    int m_request = 0;
    int m_importRequest = 0;
    int m_startPage = 1;             // catalog page the feed opened on
    int m_apiPage = 0;               // last catalog page that was requested
    bool m_discoverMore = false;     // the catalog has pages after m_apiPage
    bool m_discovering = false;      // a page is being filled right now
    bool m_browsing = true;          // false while the grid shows a search or an import
    int m_emptyPages = 0;            // consecutive catalog pages that gave no capes at all
    int m_page = 1;                  // page shown in the pager (1-based)
    QList<QList<Crafty::CapePtr>> m_pages;  // fully loaded pages
    QString m_filter;          // cape-name search: while it is set, only matching capes are added to the grid
    QSet<QString> m_scanned;   // players already looked up while discovering
    QSet<QString> m_capeKeys;  // capes already in the grid, so a shared cape shows up once

    std::unique_ptr<SkinModel> m_skin;
    SkinOpenGLWindow* m_preview = nullptr;
    QLabel* m_previewLabel = nullptr;  // fallback when OpenGL is unavailable

    QLineEdit* m_searchEdit = nullptr;
    QPushButton* m_searchBtn = nullptr;
    QPushButton* m_importBtn = nullptr;
    QListWidget* m_capeList = nullptr;
    QCheckBox* m_elytraCB = nullptr;
    QLabel* m_nameLabel = nullptr;
    QLabel* m_sourceLabel = nullptr;
    QLabel* m_statusLabel = nullptr;
    QPushButton* m_prevBtn = nullptr;
    QPushButton* m_nextBtn = nullptr;
    QLabel* m_pageLabel = nullptr;
    QPushButton* m_useBtn = nullptr;
    QPushButton* m_closeBtn = nullptr;

    QTimer* m_skeletonTimer = nullptr;
    int m_skeletonCount = 0;
    int m_skeletonTick = 0;
};
