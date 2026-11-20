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

#include "BulkUpdateDialog.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include "Application.h"
#include "CustomMessageBox.h"
#include "ProgressDialog.h"
#include "ResourceUpdateDialog.h"

#include "InstanceList.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "tasks/ConcurrentTask.h"

BulkUpdateDialog::~BulkUpdateDialog() = default;

void BulkUpdateDialog::checkAllInstances(QWidget* parent)
{
    BulkUpdateDialog dialog(parent);
    dialog.exec();
}

BulkUpdateDialog::BulkUpdateDialog(QWidget* parent) : QDialog(parent)
{
    setObjectName(QStringLiteral("BulkUpdateDialog"));
    setWindowTitle(tr("Update all instances"));

    auto* verticalLayout = new QVBoxLayout(this);

    auto* introLabel = new QLabel(tr("Check every selected instance for mod updates, then apply all found updates in one go."), this);
    introLabel->setWordWrap(true);
    verticalLayout->addWidget(introLabel);

    m_instanceTree = new QTreeWidget(this);
    m_instanceTree->setObjectName(QStringLiteral("instanceTree"));
    m_instanceTree->setColumnCount(3);
    m_instanceTree->setHeaderLabels({ tr("Instance"), tr("Mods"), tr("Status") });
    m_instanceTree->setRootIsDecorated(false);
    verticalLayout->addWidget(m_instanceTree);

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setWordWrap(true);
    verticalLayout->addWidget(m_summaryLabel);

    auto* buttonsLayout = new QHBoxLayout();

    m_checkButton = new QPushButton(tr("Check for updates"), this);
    m_checkButton->setObjectName(QStringLiteral("checkButton"));
    connect(m_checkButton, &QPushButton::clicked, this, &BulkUpdateDialog::on_checkButton_clicked);
    buttonsLayout->addWidget(m_checkButton);

    m_updateAllButton = new QPushButton(tr("Update all"), this);
    m_updateAllButton->setObjectName(QStringLiteral("updateAllButton"));
    m_updateAllButton->setEnabled(false);
    connect(m_updateAllButton, &QPushButton::clicked, this, &BulkUpdateDialog::on_updateAllButton_clicked);
    buttonsLayout->addWidget(m_updateAllButton);

    auto* closeButton = new QPushButton(tr("Close"), this);
    connect(closeButton, &QPushButton::clicked, this, &BulkUpdateDialog::close);
    buttonsLayout->addWidget(closeButton);

    verticalLayout->addLayout(buttonsLayout);

    populateInstances();
    updateSummary();

    resize(640, 420);
}

void BulkUpdateDialog::populateInstances()
{
    const int count = APPLICATION->instances()->count();
    for (int i = 0; i < count; ++i) {
        auto* mcInstance = dynamic_cast<MinecraftInstance*>(APPLICATION->instances()->at(i));
        if (!mcInstance) {
            continue;
        }

        auto entry = std::make_unique<InstanceEntry>();
        entry->instance = mcInstance;
        entry->name = mcInstance->name();
        entry->mods = mcInstance->loaderModList()->allResources();

        entry->item = new QTreeWidgetItem(m_instanceTree);
        entry->item->setText(0, entry->name);
        // Only instances with mods are pre-selected: the others have nothing to check.
        entry->item->setCheckState(0, entry->mods.isEmpty() ? Qt::Unchecked : Qt::Checked);
        entry->item->setText(1, QString::number(entry->mods.size()));
        entry->item->setText(2, entry->mods.isEmpty() ? tr("No mods") : tr("Not checked"));

        m_entries.push_back(std::move(entry));
    }

    if (m_entries.empty()) {
        m_checkButton->setEnabled(false);
    }
}

void BulkUpdateDialog::setRowStatus(InstanceEntry* entry, const QString& status)
{
    entry->item->setText(2, status);
}

void BulkUpdateDialog::updateSummary()
{
    m_summaryLabel->setText(tr("Instances with updates: %1\nUp to date: %2\nCould not be checked: %3\nSkipped: %4")
                               .arg(QString::number(m_instancesWithUpdates),
                                    QString::number(m_instancesUpToDate),
                                    QString::number(m_instancesFailed),
                                    QString::number(m_instancesSkipped)));
}

void BulkUpdateDialog::on_checkButton_clicked()
{
    runChecks();
}

void BulkUpdateDialog::runChecks()
{
    if (m_entries.empty()) {
        return;
    }

    // Global guard, shown once: mod updates are unavailable when metadata is disabled.
    if (APPLICATION->settings()->get("ModMetadataDisabled").toBool()) {
        QMessageBox::critical(this, tr("Error"), tr("Mod updates are unavailable when metadata is disabled!"));
        return;
    }

    // Warn about running instances once, for all of them, instead of per instance.
    QStringList runningNames;
    for (auto& entry : m_entries) {
        if (entry->instance->isRunning()) {
            runningNames << entry->name;
        }
    }
    if (!runningNames.isEmpty()) {
        auto response = CustomMessageBox::selectable(this,
                                                     tr("Instances are running"),
                                                     tr("The following instances are currently running:\n%1\n\n"
                                                        "Updating mods while the game is running may cause mod duplication and game crashes.\n"
                                                        "The old files may not be deleted as they are in use.\n"
                                                        "Running instances will be skipped.\n"
                                                        "Do you want to continue?")
                                                         .arg(runningNames.join('\n')),
                                                     QMessageBox::Warning,
                                                     QMessageBox::Yes | QMessageBox::No,
                                                     QMessageBox::No)
                          ->exec();
        if (response != QMessageBox::Yes) {
            return;
        }
    }

    m_checkButton->setEnabled(false);
    m_updateAllButton->setEnabled(false);
    m_updateTasks.clear();

    m_instancesWithUpdates = 0;
    m_instancesUpToDate = 0;
    m_instancesFailed = 0;
    m_instancesSkipped = 0;

    bool aborted = false;

    for (auto& entry : m_entries) {
        if (entry->item->checkState(0) != Qt::Checked) {
            continue;
        }

        // Running instances are skipped (the user was warned above, once, for all of them).
        if (entry->instance->isRunning()) {
            entry->item->setCheckState(0, Qt::Unchecked);
            setRowStatus(entry.get(), tr("Skipped (running)"));
            m_instancesSkipped++;
            continue;
        }

        if (entry->mods.isEmpty()) {
            setRowStatus(entry.get(), tr("No mods"));
            continue;
        }

        setRowStatus(entry.get(), tr("Checking..."));
        QApplication::processEvents();

        auto* profile = entry->instance->getPackProfile();

        // This dialog is never shown: it is only used to gather the update tasks.
        // checkCandidates() itself is synchronous and may show progress and confirmation
        // dialogs (metadata generation, provider selection, failed checks), just like the
        // per-instance "Check for updates" flow does.
        entry->updateDialog =
            new ResourceUpdateDialog(this, entry->instance, entry->instance->loaderModList(), entry->mods, false, profile->getModLoadersList());
        entry->updateDialog->checkCandidates();

        if (entry->updateDialog->aborted()) {
            setRowStatus(entry.get(), tr("Aborted"));
            aborted = true;
            break;
        }

        if (entry->updateDialog->noUpdates()) {
            if (entry->updateDialog->failedCount() > 0) {
                setRowStatus(entry.get(), tr("%1 could not be checked").arg(entry->updateDialog->failedCount()));
                m_instancesFailed++;
            } else {
                setRowStatus(entry.get(), tr("Up to date"));
                m_instancesUpToDate++;
            }
            continue;
        }

        const QList<ResourceDownloadTask::Ptr> tasks = entry->updateDialog->getTasks();
        for (const auto& task : tasks) {
            m_updateTasks.append(task);
        }
        entry->tasksCollected = tasks.size();

        setRowStatus(entry.get(), tr("%1 update(s) available").arg(tasks.size()));
        m_instancesWithUpdates++;
    }

    if (aborted) {
        m_updateTasks.clear();
        for (auto& entry : m_entries) {
            if (entry->tasksCollected > 0) {
                entry->tasksCollected = 0;
            }
        }
    }

    m_updateAllButton->setEnabled(!aborted && !m_updateTasks.isEmpty());
    m_checkButton->setEnabled(true);
    updateSummary();
}

void BulkUpdateDialog::on_updateAllButton_clicked()
{
    runUpdates();
}

void BulkUpdateDialog::runUpdates()
{
    if (m_updateTasks.isEmpty()) {
        return;
    }

    m_checkButton->setEnabled(false);
    m_updateAllButton->setEnabled(false);

    auto* tasks = new ConcurrentTask(tr("Download updates"), APPLICATION->settings()->get("NumberOfConcurrentDownloads").toInt());
    connect(tasks, &Task::failed, [this, tasks](const QString& reason) {
        CustomMessageBox::selectable(this, tr("Error"), reason, QMessageBox::Critical)->show();
        tasks->deleteLater();
    });
    connect(tasks, &Task::aborted, [this, tasks]() {
        CustomMessageBox::selectable(this, tr("Aborted"), tr("Download stopped by user."), QMessageBox::Information)->show();
        tasks->deleteLater();
    });
    connect(tasks, &Task::succeeded, [this, tasks]() {
        QStringList warnings = tasks->warnings();
        if (warnings.count()) {
            CustomMessageBox::selectable(this, tr("Warnings"), warnings.join('\n'), QMessageBox::Warning)->show();
        }
        tasks->deleteLater();
    });

    for (const auto& task : m_updateTasks) {
        tasks->addTask(task);
    }

    ProgressDialog loadDialog(this);
    loadDialog.setSkipButton(true, tr("Abort"));
    loadDialog.execWithTask(tasks);

    for (auto& entry : m_entries) {
        if (entry->tasksCollected > 0) {
            entry->instance->loaderModList()->update();
            setRowStatus(entry.get(), tasks->wasSuccessful() ? tr("Updated") : tr("Update failed"));
        }
    }

    m_updateTasks.clear();

    const QString headline = tasks->wasSuccessful() ? tr("Done.") : tr("The downloads did not complete successfully.");
    CustomMessageBox::selectable(this,
                                 tr("Update all instances"),
                                 tr("%1\n"
                                    "Instances updated: %2\n"
                                    "Instances up to date: %3\n"
                                    "Instances that could not be checked: %4\n"
                                    "Instances skipped: %5")
                                     .arg(headline,
                                          QString::number(m_instancesWithUpdates),
                                          QString::number(m_instancesUpToDate),
                                          QString::number(m_instancesFailed),
                                          QString::number(m_instancesSkipped)),
                                 QMessageBox::Information)
        ->exec();

    updateSummary();
}
