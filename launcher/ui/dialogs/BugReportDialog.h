// SPDX-License-Identifier: GPL-3.0-only
/*
 *  PollyMC-Continued - Minecraft Launcher
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
 */

#pragma once

#include <QDialog>
#include <QImage>
#include <QList>

class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;

/// "Report a Bug" dialog: report type, optional name and contact, description, optional screenshots, optional log.
/// OS / launcher version are added automatically and shown to the user before sending.
class BugReportDialog : public QDialog {
    Q_OBJECT

   public:
    explicit BugReportDialog(QWidget* parent = nullptr);
    ~BugReportDialog() override = default;

   private slots:
    void addScreenshotFromFile();
    void addScreenshotFromClipboard();
    void removeSelectedScreenshot();
    void updateState();
    void send();

   private:
    void addScreenshot(const QImage& image);
    QString systemInfoText() const;
    QByteArray buildPayload() const;

    QComboBox* m_type = nullptr;
    QLineEdit* m_name = nullptr;
    QLineEdit* m_contact = nullptr;
    QPlainTextEdit* m_description = nullptr;
    QLabel* m_counter = nullptr;
    QListWidget* m_shots = nullptr;
    QPushButton* m_addShot = nullptr;
    QPushButton* m_pasteShot = nullptr;
    QPushButton* m_removeShot = nullptr;
    QCheckBox* m_includeLog = nullptr;
    QDialogButtonBox* m_buttons = nullptr;
    QPushButton* m_sendButton = nullptr;

    QList<QByteArray> m_shotData;  // JPEG bytes, parallel to the items in m_shots
};
