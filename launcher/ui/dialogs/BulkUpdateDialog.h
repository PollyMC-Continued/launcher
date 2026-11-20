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

#include <QDialog>
#include <QList>

#include <memory>
#include <vector>

#include "ResourceDownloadTask.h"

class QLabel;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;
class MinecraftInstance;
class Resource;
class ResourceUpdateDialog;

/**
 * "Update all instances": lists every Minecraft instance with a checkbox per
 * row, checks the selected instances for mod updates (sequentially, like the
 * per-instance "Check for updates" does) and then applies every collected
 * update in a single ConcurrentTask.
 */
class BulkUpdateDialog : public QDialog {
    Q_OBJECT

   public:
    static void checkAllInstances(QWidget* parent);

    explicit BulkUpdateDialog(QWidget* parent = 0);
    ~BulkUpdateDialog() override;

   private slots:
    void on_checkButton_clicked();
    void on_updateAllButton_clicked();

   private:
    /* One per listed instance. Heap allocated so that the mod list stays at a
     * stable address: ResourceUpdateDialog holds a reference to it. */
    struct InstanceEntry {
        MinecraftInstance* instance = nullptr;
        QString name;
        QList<Resource*> mods;
        QTreeWidgetItem* item = nullptr;
        ResourceUpdateDialog* updateDialog = nullptr;
        int tasksCollected = 0;
    };

    void populateInstances();
    void runChecks();
    void runUpdates();
    void updateSummary();
    void setRowStatus(InstanceEntry* entry, const QString& status);

   private:
    std::vector<std::unique_ptr<InstanceEntry>> m_entries;
    QList<ResourceDownloadTask::Ptr> m_updateTasks;

    QTreeWidget* m_instanceTree = nullptr;
    QPushButton* m_checkButton = nullptr;
    QPushButton* m_updateAllButton = nullptr;
    QLabel* m_summaryLabel = nullptr;

    int m_instancesWithUpdates = 0;
    int m_instancesUpToDate = 0;
    int m_instancesFailed = 0;
    int m_instancesSkipped = 0;
};
