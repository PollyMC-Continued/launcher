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

#include <QDialog>

#include "minecraft/skins/CraftySkinAPI.h"

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

// Lets the user pick a cape: capes crafty.gg tracks for a player name, plus direct PNG URL
// imports. Built entirely in C++ (no .ui file), mirroring how the skin browser behaves:
// rate-limited crafty.gg requests through Crafty::API and aborted when the dialog closes.
class CapeBrowserDialog : public QDialog {
    Q_OBJECT

   public:
    explicit CapeBrowserDialog(QWidget* parent = nullptr);
    ~CapeBrowserDialog() override;

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

   private:
    void setLoading(bool loading);
    void fillList();
    void updateDetails();
    void showError(const QString& reason);
    QString displayName(const Crafty::CapePtr& cape) const;

   private:
    Crafty::API* m_api;
    QList<Crafty::CapePtr> m_capes;
    Crafty::CapePtr m_current;
    QString m_player;
    int m_request = 0;

    QLineEdit* m_searchEdit = nullptr;
    QPushButton* m_searchBtn = nullptr;
    QLineEdit* m_urlEdit = nullptr;
    QPushButton* m_importBtn = nullptr;
    QListWidget* m_capeList = nullptr;
    QLabel* m_helpLabel = nullptr;
    QLabel* m_sourceLabel = nullptr;
    QLabel* m_linkLabel = nullptr;
    QLabel* m_statusLabel = nullptr;
    QPushButton* m_useBtn = nullptr;
    QPushButton* m_closeBtn = nullptr;
};
