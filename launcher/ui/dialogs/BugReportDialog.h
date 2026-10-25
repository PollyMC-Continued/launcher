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
class QDragEnterEvent;
class QDropEvent;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QScrollArea;
class QToolButton;
class QWidget;

/// "Report a Bug" dialog: report type, optional name and contact, description, optional screenshots, optional log.
/// OS / launcher version are added automatically and shown to the user before sending.
///
/// Sized like the settings window: it never opens larger than the screen, remembers its size, can be resized, and the
/// form scrolls while the title and the Send / Cancel buttons stay in view. Images can be dragged onto the window.
class BugReportDialog : public QDialog {
    Q_OBJECT

   public:
    explicit BugReportDialog(QWidget* parent = nullptr);
    ~BugReportDialog() override = default;

    /// Fills in the form for an error the user just ran into (type "Bug", the error text, and a prompt to say what they were doing).
    void prefillError(const QString& title, const QString& details);

    void done(int result) override;

    /// Opens the report dialog pre-filled with an error. Used by the "Report Bug" buttons on error windows.
    /// Falls back to the issue tracker when no report endpoint is configured.
    static void reportError(QWidget* parent, const QString& title, const QString& details);

   protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

   private slots:
    void addScreenshotFromFile();
    void addScreenshotFromClipboard();
    void removeSelectedScreenshot();
    void updateState();
    void send();

   private:
    void addScreenshot(const QImage& image);
    void setBusy(bool busy);
    QString systemInfoText() const;
    QByteArray buildPayload() const;

    QScrollArea* m_scroll = nullptr;
    QWidget* m_form = nullptr;
    QToolButton* m_detailsToggle = nullptr;
    QLabel* m_details = nullptr;
    QLabel* m_intro = nullptr;
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
