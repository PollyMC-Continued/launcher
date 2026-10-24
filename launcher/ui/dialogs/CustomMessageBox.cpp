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

#include "CustomMessageBox.h"

#include <QPointer>
#include <QPushButton>
#include <QTimer>

#include "BuildConfig.h"
#include "ui/dialogs/BugReportDialog.h"

namespace CustomMessageBox {
QMessageBox* selectable(QWidget* parent,
                        const QString& title,
                        const QString& text,
                        QMessageBox::Icon icon,
                        QMessageBox::StandardButtons buttons,
                        QMessageBox::StandardButton defaultButton,
                        QCheckBox* checkBox,
                        bool reportable)
{
    QMessageBox* messageBox = new QMessageBox(parent);
    messageBox->setWindowTitle(title);
    messageBox->setText(text);
    messageBox->setStandardButtons(buttons);
    messageBox->setDefaultButton(defaultButton);
    messageBox->setTextInteractionFlags(Qt::TextSelectableByMouse);
    messageBox->setIcon(icon);
    messageBox->setTextInteractionFlags(Qt::TextBrowserInteraction);
    if (checkBox)
        messageBox->setCheckBox(checkBox);

    // Plain "something went wrong" windows get a way to tell the developers. Boxes with other buttons (Yes/No, Retry, ...) are left
    // alone because their callers look at which standard button was pressed.
    if (reportable && icon == QMessageBox::Critical && buttons == QMessageBox::Ok && !BuildConfig.BUG_REPORT_API_URL.isEmpty()) {
        auto* reportButton = messageBox->addButton(QObject::tr("Report Bug"), QMessageBox::ActionRole);
        reportButton->setToolTip(QObject::tr("Send this error to the PollyMC developers"));
        QObject::connect(reportButton, &QPushButton::clicked, messageBox, [parent = QPointer<QWidget>(parent), title, text] {
            // wait for this message box to close first, then open the report form
            QTimer::singleShot(0, [parent, title, text] { BugReportDialog::reportError(parent, title, text); });
        });
    }

    return messageBox;
}
}  // namespace CustomMessageBox
