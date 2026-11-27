// SPDX-License-Identifier: GPL-3.0-only
/*
 *  PollyMC-Continued - Minecraft Launcher
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
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
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *      Copyright 2026 PollyMC-Continued Contributors
 *
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *          http://www.apache.org/licenses/LICENSE-2.0
 *
 *      Unless required by applicable law or agreed to in writing, software
 *      distributed under the License is distributed on an "AS IS" BASIS,
 *      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *      See the License for the specific language governing permissions and
 *      limitations under the License.
 */

#pragma once

#include <QDialog>
#include <QModelIndex>

#include "InstanceTask.h"
#include "ui/pages/BasePageProvider.h"

namespace Ui {
class NewInstanceDialog;
}

class PageContainer;
class DialogButton;
class ImportPage;
class ModrinthPage;
class FlamePage;
class ModpackSearchBox;
class QDragEnterEvent;
class QDragLeaveEvent;
class QDropEvent;
class QHideEvent;
class QLabel;
class QMenu;
class QShowEvent;
class QToolButton;

class NewInstanceDialog : public QDialog, public BasePageProvider {
    Q_OBJECT

   public:
    explicit NewInstanceDialog(const QString& initialGroup,
                               const QString& url = QString(),
                               const QMap<QString, QString>& extra_info = {},
                               QWidget* parent = 0);
    ~NewInstanceDialog();

    void updateDialogState();

    void setSuggestedPack(const QString& name = QString(), InstanceTask* task = nullptr);
    void setSuggestedPack(const QString& name, QString version, InstanceTask* task = nullptr);
    void setSuggestedIconFromFile(const QString& path, const QString& name);
    void setSuggestedIcon(const QString& key);

    InstanceTask* extractTask();

    QString dialogTitle() override;
    QList<BasePage*> getPages() override;

    QString instName() const;
    QString instGroup() const;
    QString iconKey() const;

   public slots:
    void accept() override;
    void reject() override;

   protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

   private slots:
    void on_iconButton_clicked();
    void on_instNameTextBox_textChanged(const QString& arg1);
    void selectedPageChanged(BasePage* previous, BasePage* selected);
    void groupChosen(QAction* action);
    void newGroupRequested();

    void modpackSearchRequested(const QString& term);
    void modpackSearchResultActivated(const QString& sourceId, const QModelIndex& sourceIndex);

   private:
    void buildGroupMenu();
    void setCurrentGroup(const QString& group);
    void updatePrimaryAction();
    QString pageActionText(BasePage* page) const;
    QString pageHintText(BasePage* page) const;
    void rememberState();

    Ui::NewInstanceDialog* ui = nullptr;
    PageContainer* m_container = nullptr;
    ModpackSearchBox* m_searchBox = nullptr;
    DialogButton* m_helpButton = nullptr;
    DialogButton* m_cancelButton = nullptr;
    DialogButton* m_primaryButton = nullptr;
    QLabel* m_hintLabel = nullptr;

    QMenu* m_groupMenu = nullptr;
    QToolButton* m_groupChip = nullptr;
    QString m_currentGroup;
    QStringList m_groups;

    QString InstIconKey;
    ImportPage* importPage = nullptr;
    ModrinthPage* modrinthPage = nullptr;
    FlamePage* flamePage = nullptr;
    std::unique_ptr<InstanceTask> creationTask;

    bool importIcon = false;
    QString importIconPath;
    QString importIconName;

    QString importVersion;

    QString m_searchTerm;

    void importIconNow();
};
