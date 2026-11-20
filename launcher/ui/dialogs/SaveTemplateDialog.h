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

#include "BaseInstance.h"

class QCheckBox;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QPushButton;

struct SaveTemplateOptions {
    QString name;
    QString iconKey;
    bool includeMods = true;
    bool includeConfigs = true;
    bool includeResPacks = false;
    bool includeShaderPacks = false;
    bool includeServers = false;
    bool overwrite = false;
};

class SaveTemplateDialog : public QDialog {
    Q_OBJECT

   public:
    explicit SaveTemplateDialog(BaseInstance* original, QWidget* parent = 0);

    SaveTemplateOptions getOptions() const;

    /// The whitelist entries matching the current checkbox state, for SaveTemplateTask.
    QStringList includeFolders() const;

    void accept() override;

   private slots:
    void on_iconButton_clicked();
    void on_nameTextBox_textChanged(const QString& arg1);

   private:
    void updateDialogState();

   private:
    BaseInstance* m_original;
    QString m_iconKey;
    bool m_overwrite = false;

    QLineEdit* m_nameTextBox = nullptr;
    QPushButton* m_iconButton = nullptr;
    QCheckBox* m_modsCheckbox = nullptr;
    QCheckBox* m_configsCheckbox = nullptr;
    QCheckBox* m_resPacksCheckbox = nullptr;
    QCheckBox* m_shaderPacksCheckbox = nullptr;
    QCheckBox* m_serversCheckbox = nullptr;
    QLabel* m_folderLabel = nullptr;
    QDialogButtonBox* m_buttonBox = nullptr;
};
