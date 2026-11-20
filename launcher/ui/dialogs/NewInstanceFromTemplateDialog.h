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

class QComboBox;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

class NewInstanceFromTemplateDialog : public QDialog {
    Q_OBJECT

   public:
    /// Shows the dialog and, if a template is chosen, creates the instance.
    /// Returns true when a new instance was created successfully.
    static bool createFromTemplate(QWidget* parent);

    explicit NewInstanceFromTemplateDialog(QWidget* parent = 0);

   private slots:
    void on_templateTree_currentItemChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous);
    void on_nameTextBox_textChanged(const QString& arg1);
    void on_iconButton_clicked();
    void on_deleteTemplateButton_clicked();

   private:
    struct TemplateInfo {
        QString dir;
        QString name;
        QString created;
        QString minecraftVersion;
        QString iconKey;
    };

    void scanTemplates();
    void selectTemplate(int index);
    void updateDialogState();
    bool createInstance();

   private:
    QList<TemplateInfo> m_templates;
    int m_selectedIndex = -1;
    QString m_iconKey;
    QString m_lastAutoName;
    bool m_nameEdited = false;

    QTreeWidget* m_templateTree = nullptr;
    QLineEdit* m_nameTextBox = nullptr;
    QComboBox* m_groupBox = nullptr;
    QPushButton* m_iconButton = nullptr;
    QPushButton* m_deleteTemplateButton = nullptr;
    QDialogButtonBox* m_buttonBox = nullptr;
};
