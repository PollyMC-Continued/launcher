// SPDX-License-Identifier: GPL-3.0-only
/*
 *  PollyMC-Continued - Minecraft Launcher
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
 *  Copyright (C) 2023 TheKodeToad <TheKodeToad@proton.me>
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

#include "SaveTemplateDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSize>
#include <QVBoxLayout>

#include "Application.h"
#include "CustomMessageBox.h"
#include "FileSystem.h"
#include "IconPickerDialog.h"
#include "InstanceTemplateTask.h"
#include "icons/IconList.h"
#include "ui/GuiUtil.h"

SaveTemplateDialog::SaveTemplateDialog(BaseInstance* original, QWidget* parent) : QDialog(parent), m_original(original)
{
    setObjectName(QStringLiteral("SaveTemplateDialog"));
    setWindowTitle(tr("Save instance as template"));

    auto* verticalLayout = new QVBoxLayout(this);

    auto* nameLayout = new QGridLayout();

    auto* nameLabel = new QLabel(tr("Template name:"), this);
    m_nameTextBox = new QLineEdit(this);
    m_nameTextBox->setObjectName(QStringLiteral("nameTextBox"));
    m_nameTextBox->setFocus();
    nameLayout->addWidget(nameLabel, 0, 0);
    nameLayout->addWidget(m_nameTextBox, 0, 1);

    m_iconButton = new QPushButton(this);
    m_iconButton->setObjectName(QStringLiteral("iconButton"));
    m_iconButton->setIconSize(QSize(64, 64));
    m_iconButton->setFlat(true);
    m_iconKey = original->iconKey();
    m_iconButton->setIcon(APPLICATION->icons()->getIcon(m_iconKey));
    m_iconButton->setToolTip(tr("Change the template's icon."));
    nameLayout->addWidget(m_iconButton, 0, 2, 2, 1, Qt::AlignTop);

    m_nameTextBox->setText(SaveTemplateTask::sanitizeTemplateDirName(original->name()));
    verticalLayout->addLayout(nameLayout);

    m_modsCheckbox = new QCheckBox(tr("Mods (mods/)"), this);
    m_modsCheckbox->setChecked(true);
    verticalLayout->addWidget(m_modsCheckbox);

    m_configsCheckbox = new QCheckBox(tr("Configs (config/, options.txt)"), this);
    m_configsCheckbox->setChecked(true);
    verticalLayout->addWidget(m_configsCheckbox);

    m_resPacksCheckbox = new QCheckBox(tr("Resource packs (resourcepacks/)"), this);
    m_resPacksCheckbox->setChecked(false);
    verticalLayout->addWidget(m_resPacksCheckbox);

    m_shaderPacksCheckbox = new QCheckBox(tr("Shader packs (shaderpacks/)"), this);
    m_shaderPacksCheckbox->setChecked(false);
    verticalLayout->addWidget(m_shaderPacksCheckbox);

    m_serversCheckbox = new QCheckBox(tr("Servers list (servers.dat)"), this);
    m_serversCheckbox->setChecked(false);
    verticalLayout->addWidget(m_serversCheckbox);

    auto* neverIncludedLabel = new QLabel(tr("Worlds, logs, screenshots and crash reports are never included."), this);
    neverIncludedLabel->setWordWrap(true);
    verticalLayout->addWidget(neverIncludedLabel);

    m_folderLabel = new QLabel(this);
    m_folderLabel->setWordWrap(true);
    verticalLayout->addWidget(m_folderLabel);

    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
    m_buttonBox->button(QDialogButtonBox::Ok)->setText(tr("OK"));
    m_buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &SaveTemplateDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &SaveTemplateDialog::reject);
    verticalLayout->addWidget(m_buttonBox);

    connect(m_nameTextBox, &QLineEdit::textChanged, this, &SaveTemplateDialog::on_nameTextBox_textChanged);
    connect(m_iconButton, &QPushButton::clicked, this, &SaveTemplateDialog::on_iconButton_clicked);

    updateDialogState();

    resize(minimumSizeHint());
    layout()->setSizeConstraint(QLayout::SetFixedSize);

    // remember the window size between runs, like the settings window does
    GuiUtil::rememberGeometry(this, "SaveTemplateGeometry");
}

SaveTemplateOptions SaveTemplateDialog::getOptions() const
{
    SaveTemplateOptions options;
    options.name = m_nameTextBox->text().trimmed();
    options.iconKey = m_iconKey;
    options.includeMods = m_modsCheckbox->isChecked();
    options.includeConfigs = m_configsCheckbox->isChecked();
    options.includeResPacks = m_resPacksCheckbox->isChecked();
    options.includeShaderPacks = m_shaderPacksCheckbox->isChecked();
    options.includeServers = m_serversCheckbox->isChecked();
    options.overwrite = m_overwrite;
    return options;
}

QStringList SaveTemplateDialog::includeFolders() const
{
    QStringList folders;
    if (m_modsCheckbox->isChecked())
        folders << "mods";
    if (m_configsCheckbox->isChecked())
        folders << "configs";
    if (m_resPacksCheckbox->isChecked())
        folders << "resourcepacks";
    if (m_shaderPacksCheckbox->isChecked())
        folders << "shaderpacks";
    if (m_serversCheckbox->isChecked())
        folders << "servers";
    return folders;
}

void SaveTemplateDialog::accept()
{
    const QString safeName = SaveTemplateTask::sanitizeTemplateDirName(m_nameTextBox->text().trimmed());
    const QString templateDir = FS::PathCombine(SaveTemplateTask::templatesRoot(), safeName);

    if (QFileInfo::exists(templateDir)) {
        auto response = CustomMessageBox::selectable(this,
                                                     tr("Template already exists"),
                                                     tr("A template named %1 already exists.\n"
                                                        "Do you want to overwrite it?")
                                                         .arg(safeName),
                                                     QMessageBox::Warning,
                                                     QMessageBox::Yes | QMessageBox::No,
                                                     QMessageBox::No)
                            ->exec();

        if (response != QMessageBox::Yes) {
            return;
        }

        m_overwrite = true;
    }

    QDialog::accept();
}

void SaveTemplateDialog::on_iconButton_clicked()
{
    IconPickerDialog dlg(this);
    dlg.execWithSelection(m_iconKey);

    if (dlg.result() == QDialog::Accepted) {
        m_iconKey = dlg.selectedIconKey;
        m_iconButton->setIcon(APPLICATION->icons()->getIcon(m_iconKey));
    }
}

void SaveTemplateDialog::on_nameTextBox_textChanged([[maybe_unused]] const QString& arg1)
{
    updateDialogState();
}

void SaveTemplateDialog::updateDialogState()
{
    const QString safeName = SaveTemplateTask::sanitizeTemplateDirName(m_nameTextBox->text().trimmed());
    auto* okButton = m_buttonBox->button(QDialogButtonBox::Ok);
    const bool allowOK = !safeName.isEmpty();
    if (okButton->isEnabled() != allowOK) {
        okButton->setEnabled(allowOK);
    }

    m_folderLabel->setText(tr("The template will be saved in:\n%1").arg(FS::PathCombine(SaveTemplateTask::templatesRoot(), safeName)));
}
