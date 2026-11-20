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

#include "NewInstanceFromTemplateDialog.h"

#include <QComboBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFormLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSize>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include "Application.h"
#include "CustomMessageBox.h"
#include "FileSystem.h"
#include "IconPickerDialog.h"
#include "InstanceList.h"
#include "InstanceTemplateTask.h"
#include "ProgressDialog.h"
#include "QObjectPtr.h"
#include "icons/IconList.h"
#include "ui/GuiUtil.h"

bool NewInstanceFromTemplateDialog::createFromTemplate(QWidget* parent)
{
    NewInstanceFromTemplateDialog dialog(parent);
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }
    return dialog.createInstance();
}

NewInstanceFromTemplateDialog::NewInstanceFromTemplateDialog(QWidget* parent) : QDialog(parent)
{
    setObjectName(QStringLiteral("NewInstanceFromTemplateDialog"));
    setWindowTitle(tr("Create instance from template"));

    auto* verticalLayout = new QVBoxLayout(this);

    auto* introLabel = new QLabel(tr("Select a template to create a new instance from:"), this);
    verticalLayout->addWidget(introLabel);

    m_templateTree = new QTreeWidget(this);
    m_templateTree->setObjectName(QStringLiteral("templateTree"));
    m_templateTree->setColumnCount(3);
    m_templateTree->setHeaderLabels({ tr("Name"), tr("Created"), tr("Minecraft version") });
    m_templateTree->setRootIsDecorated(false);
    m_templateTree->setSelectionMode(QAbstractItemView::SingleSelection);
    verticalLayout->addWidget(m_templateTree);

    auto* instanceLayout = new QFormLayout();

    m_nameTextBox = new QLineEdit(this);
    m_nameTextBox->setObjectName(QStringLiteral("nameTextBox"));
    instanceLayout->addRow(tr("Instance name:"), m_nameTextBox);

    m_groupBox = new QComboBox(this);
    m_groupBox->setObjectName(QStringLiteral("groupBox"));
    m_groupBox->setEditable(true);
    instanceLayout->addRow(tr("Group:"), m_groupBox);

    m_iconButton = new QPushButton(this);
    m_iconButton->setObjectName(QStringLiteral("iconButton"));
    m_iconButton->setIconSize(QSize(64, 64));
    m_iconButton->setFlat(true);
    m_iconButton->setToolTip(tr("Change the new instance's icon."));
    instanceLayout->addRow(tr("Icon:"), m_iconButton);

    verticalLayout->addLayout(instanceLayout);

    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
    m_buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Create instance"));
    m_buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &NewInstanceFromTemplateDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &NewInstanceFromTemplateDialog::reject);

    m_deleteTemplateButton = new QPushButton(tr("Delete template"), this);
    m_deleteTemplateButton->setObjectName(QStringLiteral("deleteTemplateButton"));
    m_buttonBox->addButton(m_deleteTemplateButton, QDialogButtonBox::DestructiveRole);
    verticalLayout->addWidget(m_buttonBox);

    QStringList groups = APPLICATION->instances()->getGroups();
    groups.prepend("");
    m_groupBox->addItems(groups);
    m_groupBox->setCurrentIndex(0);
    m_groupBox->lineEdit()->setPlaceholderText(tr("No group"));

    m_iconKey = "default";
    m_iconButton->setIcon(APPLICATION->icons()->getIcon(m_iconKey));

    connect(m_templateTree, &QTreeWidget::currentItemChanged, this, &NewInstanceFromTemplateDialog::on_templateTree_currentItemChanged);
    connect(m_nameTextBox, &QLineEdit::textChanged, this, &NewInstanceFromTemplateDialog::on_nameTextBox_textChanged);
    connect(m_iconButton, &QPushButton::clicked, this, &NewInstanceFromTemplateDialog::on_iconButton_clicked);
    connect(m_deleteTemplateButton, &QPushButton::clicked, this, &NewInstanceFromTemplateDialog::on_deleteTemplateButton_clicked);

    scanTemplates();

    updateDialogState();

    resize(560, 420);

    // remember the window size between runs, like the settings window does
    GuiUtil::rememberGeometry(this, "NewInstanceFromTemplateGeometry");
}

void NewInstanceFromTemplateDialog::scanTemplates()
{
    m_templates.clear();
    m_templateTree->clear();

    const QDir templatesDir(SaveTemplateTask::templatesRoot());
    if (!templatesDir.exists()) {
        updateDialogState();
        return;
    }

    const QFileInfoList entries = templatesDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable, QDir::Name);
    for (const QFileInfo& entry : entries) {
        QFile manifestFile(FS::PathCombine(entry.absoluteFilePath(), "template.json"));
        if (!manifestFile.exists() || !manifestFile.open(QIODevice::ReadOnly)) {
            continue;
        }

        QJsonParseError jsonError;
        const QJsonDocument manifest = QJsonDocument::fromJson(manifestFile.readAll(), &jsonError);
        manifestFile.close();
        if (jsonError.error != QJsonParseError::NoError || !manifest.isObject()) {
            qWarning() << "Invalid template manifest in" << entry.absoluteFilePath();
            continue;
        }

        const QJsonObject object = manifest.object();

        TemplateInfo info;
        info.dir = entry.absoluteFilePath();
        info.name = object["name"].toString(entry.fileName());
        info.created = object["created"].toString();
        info.minecraftVersion = object["minecraftVersion"].toString();
        info.iconKey = object["iconKey"].toString();
        if (info.iconKey.isEmpty()) {
            info.iconKey = "default";
        }

        QString created = info.created;
        const QDateTime createdAt = QDateTime::fromString(info.created, Qt::ISODate);
        if (createdAt.isValid()) {
            created = QLocale::system().toString(createdAt, QLocale::ShortFormat);
        }

        auto* item = new QTreeWidgetItem(m_templateTree);
        item->setText(0, info.name);
        item->setText(1, created);
        item->setText(2, info.minecraftVersion);
        item->setData(0, Qt::UserRole, static_cast<int>(m_templates.size()));

        m_templates.append(info);
    }

    m_selectedIndex = -1;
    if (!m_templates.isEmpty()) {
        m_templateTree->setCurrentItem(m_templateTree->topLevelItem(0));
    } else {
        auto* messageItem = new QTreeWidgetItem(m_templateTree);
        messageItem->setText(0, tr("No templates found."));
        messageItem->setFlags(Qt::NoItemFlags);
        m_templateTree->addTopLevelItem(messageItem);
        updateDialogState();
    }
}

void NewInstanceFromTemplateDialog::selectTemplate(int index)
{
    if (index < 0 || index >= m_templates.size()) {
        m_selectedIndex = -1;
        updateDialogState();
        return;
    }

    m_selectedIndex = index;
    const TemplateInfo& info = m_templates.at(index);

    m_iconKey = info.iconKey;
    m_iconButton->setIcon(APPLICATION->icons()->getIcon(m_iconKey));

    // Only auto-fill the instance name if the user has not typed their own.
    if (!m_nameEdited) {
        m_lastAutoName = info.name;
        m_nameTextBox->setText(info.name);
    }
}

void NewInstanceFromTemplateDialog::updateDialogState()
{
    const bool templateSelected = (m_selectedIndex >= 0 && m_selectedIndex < m_templates.size());
    const bool nameOk = !m_nameTextBox->text().trimmed().isEmpty();

    auto* okButton = m_buttonBox->button(QDialogButtonBox::Ok);
    okButton->setEnabled(templateSelected && nameOk);
    m_deleteTemplateButton->setEnabled(templateSelected);
}

bool NewInstanceFromTemplateDialog::createInstance()
{
    if (m_selectedIndex < 0 || m_selectedIndex >= m_templates.size()) {
        return false;
    }
    const TemplateInfo& info = m_templates.at(m_selectedIndex);

    const QString instName = m_nameTextBox->text().trimmed();
    if (instName.isEmpty()) {
        return false;
    }

    auto* rawTask = new InstantiateTemplateTask(info.dir, instName, m_groupBox->currentText(), m_iconKey);
    unique_qobject_ptr<Task> task(APPLICATION->instances()->wrapInstanceTask(rawTask));

    connect(task.get(), &Task::failed, [this](const QString& reason) {
        CustomMessageBox::selectable(this, tr("Error"), reason, QMessageBox::Critical)->show();
    });
    connect(task.get(), &Task::aborted, [this]() {
        CustomMessageBox::selectable(this, tr("Task aborted"), tr("The task has been aborted by the user."), QMessageBox::Information)->show();
    });

    ProgressDialog progressDialog(this);
    progressDialog.setSkipButton(true, tr("Abort"));
    progressDialog.execWithTask(task.get());

    return task->wasSuccessful();
}

void NewInstanceFromTemplateDialog::on_templateTree_currentItemChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous)
{
    if (current == nullptr) {
        m_selectedIndex = -1;
        updateDialogState();
        return;
    }

    const QVariant indexData = current->data(0, Qt::UserRole);
    if (!indexData.isValid()) {
        m_selectedIndex = -1;
        updateDialogState();
        return;
    }

    selectTemplate(indexData.toInt());
}

void NewInstanceFromTemplateDialog::on_nameTextBox_textChanged(const QString& arg1)
{
    if (!arg1.trimmed().isEmpty() && arg1 != m_lastAutoName) {
        m_nameEdited = true;
    }
    updateDialogState();
}

void NewInstanceFromTemplateDialog::on_iconButton_clicked()
{
    IconPickerDialog dlg(this);
    dlg.execWithSelection(m_iconKey);

    if (dlg.result() == QDialog::Accepted) {
        m_iconKey = dlg.selectedIconKey;
        m_iconButton->setIcon(APPLICATION->icons()->getIcon(m_iconKey));
    }
}

void NewInstanceFromTemplateDialog::on_deleteTemplateButton_clicked()
{
    if (m_selectedIndex < 0 || m_selectedIndex >= m_templates.size()) {
        return;
    }
    const TemplateInfo& info = m_templates.at(m_selectedIndex);

    auto response = CustomMessageBox::selectable(this,
                                                 tr("Delete template"),
                                                 tr("Are you sure you want to delete the template %1?\n"
                                                    "This cannot be undone.")
                                                     .arg(info.name),
                                                 QMessageBox::Warning,
                                                 QMessageBox::Yes | QMessageBox::No,
                                                 QMessageBox::No)
                        ->exec();

    if (response != QMessageBox::Yes) {
        return;
    }

    if (!FS::deletePath(info.dir)) {
        CustomMessageBox::selectable(this, tr("Error"), tr("Could not delete the template folder:\n%1").arg(info.dir), QMessageBox::Critical)
            ->exec();
        return;
    }

    scanTemplates();
}
