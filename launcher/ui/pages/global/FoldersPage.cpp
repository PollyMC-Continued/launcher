// SPDX-License-Identifier: GPL-3.0-only
/*
 *  PollyMC-Continued - Minecraft Launcher
 *  Copyright (c) 2022 Jamie Mansfield <jmansfield@cadixdev.org>
 *  Copyright (c) 2022 dada513 <dada513@protonmail.com>
 *  Copyright (C) 2022 Tayou <git@tayou.org>
 *  Copyright (C) 2024 TheKodeToad <TheKodeToad@proton.me>
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

#include "FoldersPage.h"
#include "ui_FoldersPage.h"

#include <QDir>
#include <QFileDialog>
#include <QMessageBox>

#include <FileSystem.h>
#include "Application.h"
#include "BuildConfig.h"
#include "DesktopServices.h"
#include "SettingDescription.h"
#include "settings/SettingsObject.h"

FoldersPage::FoldersPage(QWidget* parent) : QWidget(parent), ui(new Ui::FoldersPage)
{
    ui->setupUi(this);
    styleSettingDescriptions(this);
    loadSettings();
}

FoldersPage::~FoldersPage()
{
    delete ui;
}

bool FoldersPage::apply()
{
    applySettings();
    return true;
}

void FoldersPage::on_instDirBrowseBtn_clicked()
{
    QString raw_dir = QFileDialog::getExistingDirectory(this, tr("Instance Folder"), ui->instDirTextBox->text());

    // do not allow current dir - it's dirty. Do not allow dirs that don't exist
    if (!raw_dir.isEmpty() && QDir(raw_dir).exists()) {
        QString cooked_dir = FS::NormalizePath(raw_dir);
        if (FS::checkProblemticPathJava(QDir(cooked_dir))) {
            QMessageBox warning;
            warning.setText(
                tr("You're trying to specify an instance folder which\'s path "
                   "contains at least one \'!\'. "
                   "Java is known to cause problems if that is the case, your "
                   "instances (probably) won't start!"));
            warning.setInformativeText(
                tr("Do you really want to use this path? "
                   "Selecting \"No\" will close this and not alter your instance path."));
            warning.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
            int result = warning.exec();
            if (result == QMessageBox::Ok) {
                ui->instDirTextBox->setText(cooked_dir);
            }
        } else if (DesktopServices::isFlatpak() && raw_dir.startsWith("/run/user")) {
            QMessageBox warning;
            warning.setText(tr("You're trying to specify an instance folder "
                               "which was granted temporarily via Flatpak.\n"
                               "This is known to cause problems. "
                               "After a restart the launcher might break, "
                               "because it will no longer have access to that directory.\n\n"
                               "Granting %1 access to it via Flatseal is recommended.")
                                .arg(BuildConfig.LAUNCHER_DISPLAYNAME));
            warning.setInformativeText(tr("Do you want to proceed anyway?"));
            warning.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
            int result = warning.exec();
            if (result == QMessageBox::Ok) {
                ui->instDirTextBox->setText(cooked_dir);
            }
        } else {
            ui->instDirTextBox->setText(cooked_dir);
        }
    }
}

void FoldersPage::on_iconsDirBrowseBtn_clicked()
{
    QString raw_dir = QFileDialog::getExistingDirectory(this, tr("Icons Folder"), ui->iconsDirTextBox->text());

    // do not allow current dir - it's dirty. Do not allow dirs that don't exist
    if (!raw_dir.isEmpty() && QDir(raw_dir).exists()) {
        QString cooked_dir = FS::NormalizePath(raw_dir);
        ui->iconsDirTextBox->setText(cooked_dir);
    }
}

void FoldersPage::on_modsDirBrowseBtn_clicked()
{
    QString raw_dir = QFileDialog::getExistingDirectory(this, tr("Mods Folder"), ui->modsDirTextBox->text());

    // do not allow current dir - it's dirty. Do not allow dirs that don't exist
    if (!raw_dir.isEmpty() && QDir(raw_dir).exists()) {
        QString cooked_dir = FS::NormalizePath(raw_dir);
        ui->modsDirTextBox->setText(cooked_dir);
    }
}

void FoldersPage::on_downloadsDirBrowseBtn_clicked()
{
    QString raw_dir = QFileDialog::getExistingDirectory(this, tr("Downloads Folder"), ui->downloadsDirTextBox->text());

    if (!raw_dir.isEmpty() && QDir(raw_dir).exists()) {
        QString cooked_dir = FS::NormalizePath(raw_dir);
        ui->downloadsDirTextBox->setText(cooked_dir);
    }
}

void FoldersPage::on_javaDirBrowseBtn_clicked()
{
    QString raw_dir = QFileDialog::getExistingDirectory(this, tr("Java Folder"), ui->javaDirTextBox->text());

    if (!raw_dir.isEmpty() && QDir(raw_dir).exists()) {
        QString cooked_dir = FS::NormalizePath(raw_dir);
        ui->javaDirTextBox->setText(cooked_dir);
    }
}

void FoldersPage::on_skinsDirBrowseBtn_clicked()
{
    QString raw_dir = QFileDialog::getExistingDirectory(this, tr("Skins Folder"), ui->skinsDirTextBox->text());

    // do not allow current dir - it's dirty. Do not allow dirs that don't exist
    if (!raw_dir.isEmpty() && QDir(raw_dir).exists()) {
        QString cooked_dir = FS::NormalizePath(raw_dir);
        ui->skinsDirTextBox->setText(cooked_dir);
    }
}

void FoldersPage::applySettings()
{
    auto s = APPLICATION->settings();

    // TODO: Offer to move instances to new instance folder.
    s->set("InstanceDir", ui->instDirTextBox->text());
    s->set("CentralModsDir", ui->modsDirTextBox->text());
    s->set("IconsDir", ui->iconsDirTextBox->text());
    s->set("DownloadsDir", ui->downloadsDirTextBox->text());
    s->set("SkinsDir", ui->skinsDirTextBox->text());
    s->set("JavaDir", ui->javaDirTextBox->text());
    s->set("DownloadsDirWatchRecursive", ui->downloadsDirWatchRecursiveCheckBox->isChecked());
    s->set("MoveModsFromDownloadsDir", ui->downloadsDirMoveCheckBox->isChecked());
}

void FoldersPage::loadSettings()
{
    auto s = APPLICATION->settings();

    ui->instDirTextBox->setText(s->get("InstanceDir").toString());
    ui->modsDirTextBox->setText(s->get("CentralModsDir").toString());
    ui->iconsDirTextBox->setText(s->get("IconsDir").toString());
    ui->downloadsDirTextBox->setText(s->get("DownloadsDir").toString());
    ui->skinsDirTextBox->setText(s->get("SkinsDir").toString());
    ui->javaDirTextBox->setText(s->get("JavaDir").toString());
    ui->downloadsDirWatchRecursiveCheckBox->setChecked(s->get("DownloadsDirWatchRecursive").toBool());
    ui->downloadsDirMoveCheckBox->setChecked(s->get("MoveModsFromDownloadsDir").toBool());
}

void FoldersPage::retranslate()
{
    ui->retranslateUi(this);
}
