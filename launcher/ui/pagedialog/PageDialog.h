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
#include <QElapsedTimer>
#include "ui/pages/BasePageProvider.h"

class PageContainer;
class QPushButton;
class PageDialog : public QDialog {
    Q_OBJECT
   public:
    explicit PageDialog(BasePageProvider* pageProvider, QString defaultId = QString(), QWidget* parent = 0);
    virtual ~PageDialog() {}

   signals:
    void applied();

   protected:
    void changeEvent(QEvent* event) override;

   private:
    void accept() override;
    void closeEvent(QCloseEvent* event) override;
    bool handleClose();

    // "OK" turns into "Apply" as soon as something was changed; applying turns it back into "OK".
    void watchPageWidgets();
    void markDirty();
    void setDirty(bool dirty);
    void updateButtonTexts();
    void applyChanges();
    void okClicked();

   private:
    PageContainer* m_container;
    QPushButton* m_okButton = nullptr;
    QPushButton* m_cancelButton = nullptr;
    QPushButton* m_helpButton = nullptr;
    bool m_dirty = false;
    QElapsedTimer m_lastButtonClick;  // a Browse-style button was clicked: text it fills in afterwards is a user edit
};
