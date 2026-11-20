// SPDX-License-Identifier: GPL-3.0-only
/*
 *  PollyMC-Continued - Minecraft Launcher
 *  Copyright (c) 2023-2024 Trial97 <alexandru.tripon97@gmail.com>
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

#include <QDialog>
#include <QImage>
#include <QItemSelection>
#include <QLabel>
#include <QPixmap>

#include "minecraft/auth/MinecraftAccount.h"
#include "minecraft/skins/SkinList.h"
#include "minecraft/skins/SkinModel.h"
#include "ui/dialogs/skins/draw/SkinOpenGLWindow.h"

namespace Ui {
class SkinManageDialog;
}
namespace Crafty {
class API;
}
class SkinManageDialog : public QDialog, public SkinProvider {
    Q_OBJECT
   public:
    explicit SkinManageDialog(QWidget* parent, MinecraftAccountPtr acct);
    virtual ~SkinManageDialog();
    void resizeEvent(QResizeEvent* event) override;

    virtual SkinModel* getSelectedSkin() override;
    virtual QHash<QString, QImage> capes() override;

   public slots:
    void selectionChanged(QItemSelection, QItemSelection);
    void activated(QModelIndex);
    void delayed_scroll(QModelIndex);
    void on_fileBtn_clicked();
    void on_importBtn_clicked();
    void importFromUrl(const QString& urlText);
    void importFromUser(const QString& user);
    void on_viewOnlineBtn_clicked();
    void accept() override;
    void on_capeCombo_currentIndexChanged(int index);
    void on_steveBtn_toggled(bool checked);
    void resetSkin();
    void openSkinsFolder();
    void show_context_menu(const QPoint& pos);
    bool eventFilter(QObject* obj, QEvent* ev) override;
    void on_action_Rename_Skin_triggered(bool checked);
    void on_action_Delete_Skin_triggered(bool checked);
    void on_offlineBrowseBtn_clicked();
    void on_offlineFileBtn_clicked();
    void on_offlineUrlBtn_clicked();
    void on_offlineRemoveBtn_clicked();

   private:
    void setupCapes();
    void setupOfflineCape();
    void applyOfflineCape(const QImage& cape, const QString& name);
    void updateOfflineCapeUi();

   private:
    MinecraftAccountPtr m_acct;
    Ui::SkinManageDialog* m_ui;
    SkinList m_list;
    QString m_selectedSkinKey;
    QHash<QString, QImage> m_capes;
    QHash<QString, int> m_capesIdx;
    SkinOpenGLWindow* m_skinPreview = nullptr;
    QLabel* m_skinPreviewLabel = nullptr;
    Crafty::API* m_capeApi = nullptr;
};
