// SPDX-License-Identifier: GPL-3.0-only
/*
 *  PollyMC-Continued - Minecraft Launcher
 *  Copyright (C) 2025 Octol1ttle <l1ttleofficial@outlook.com>
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

#include "NetworkJobFailedDialog.h"

#include <QClipboard>
#include <QFileDialog>
#include <QLabel>
#include <QPushButton>
#include <QShortcut>
#include <QStandardPaths>
#include <utility>

#include "Application.h"
#include "BuildConfig.h"
#include "DesktopServices.h"
#include "ui/GuiUtil.h"
#include "ui/dialogs/BugReportDialog.h"
#include "ui_NetworkJobFailedDialog.h"

namespace {
// Same heuristic as FileSink's access hint: disk errors that name "denied" or "access".
bool looksLikePermissionError(const QString& error)
{
    return error.contains("denied", Qt::CaseInsensitive) || error.contains("access", Qt::CaseInsensitive);
}
}  // namespace

NetworkJobFailedDialog::NetworkJobFailedDialog(const QString& jobName, const int attempts, const int requests, const int failed, QWidget* parent)
    : QDialog(parent), m_ui(new Ui::NetworkJobFailedDialog), m_jobName(jobName)
{
    m_ui->setupUi(this);
    m_ui->failLabel->setText(m_ui->failLabel->text().arg(jobName));
    if (failed == requests) {
        m_ui->requestCountLabel->setText(tr("All %1 requests failed after %2 attempts").arg(failed).arg(attempts));
    } else if (failed < requests / 2) {
        m_ui->requestCountLabel->setText(
            tr("Only %2 of %1 requests failed (after %3 attempts)").arg(requests).arg(failed).arg(attempts));
    } else {
        m_ui->requestCountLabel->setText(
            tr("Only %2 of %1 requests succeeded (after %3 attempts)").arg(requests).arg(requests - failed).arg(attempts));
    }

    m_ui->detailsTable->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_ui->detailsTable->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    const auto* copyShortcut = new QShortcut(QKeySequence::Copy, m_ui->detailsTable);
    connect(copyShortcut, &QShortcut::activated, this, &NetworkJobFailedDialog::copyUrl);

    const auto* copyButton = m_ui->dialogButtonBox->addButton(tr("Copy URL"), QDialogButtonBox::ActionRole);
    connect(copyButton, &QPushButton::clicked, this, &NetworkJobFailedDialog::copyUrl);

    if (!BuildConfig.BUG_REPORT_API_URL.isEmpty()) {
        auto* reportButton = m_ui->dialogButtonBox->addButton(tr("Report Bug"), QDialogButtonBox::ActionRole);
        reportButton->setToolTip(tr("Send this error to the PollyMC developers"));
        connect(reportButton, &QPushButton::clicked, this, &NetworkJobFailedDialog::reportBug);
    }

    // Antivirus repair options: only shown once a failed request actually looks like
    // a "Permission denied" (see addFailedRequest).
    m_permissionHint = new QLabel(
        tr("This looks like antivirus software (or ransomware protection) blocking the launcher's temporary files. "
           "You can retry through a different temporary folder, or open the guide on letting your antivirus "
           "leave the launcher alone."),
        this);
    m_permissionHint->setWordWrap(true);
    m_permissionHint->setStyleSheet(QStringLiteral("color: gray;"));
    m_permissionHint->setVisible(false);
    m_ui->verticalLayout->insertWidget(3, m_permissionHint);

    m_stagingButton = m_ui->dialogButtonBox->addButton(tr("Retry with a different temp folder…"), QDialogButtonBox::ActionRole);
    m_stagingButton->setToolTip(tr("Pick a folder the launcher will use to stage downloads, then retry. "
                                   "Choose one that your antivirus does not watch, such as a folder in your home directory."));
    m_stagingButton->setVisible(false);
    connect(m_stagingButton, &QPushButton::clicked, this, &NetworkJobFailedDialog::retryWithDifferentTempFolder);

    if (!BuildConfig.AV_EXCLUSION_URL.isEmpty()) {
        m_avGuideButton = m_ui->dialogButtonBox->addButton(tr("Open the antivirus exclusion guide"), QDialogButtonBox::ActionRole);
        m_avGuideButton->setToolTip(tr("Opens a guide explaining how to add the launcher to your antivirus exclusions."));
        m_avGuideButton->setVisible(false);
        connect(m_avGuideButton, &QPushButton::clicked, this, &NetworkJobFailedDialog::openAntivirusGuide);
    }

    connect(m_ui->dialogButtonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_ui->dialogButtonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // remember the window size between runs, like the settings window does
    GuiUtil::rememberGeometry(this, "NetworkErrorGeometry");
}

NetworkJobFailedDialog::~NetworkJobFailedDialog()
{
    delete m_ui;
}

void NetworkJobFailedDialog::addFailedRequest(const QUrl& url, QString error) const
{
    if (!m_permissionTrouble && looksLikePermissionError(error)) {
        m_permissionTrouble = true;
        // the buttons and hint exist by now - the first failed request is added after construction
        showPermissionRepairOptions();
    }

    auto* item = new QTreeWidgetItem(m_ui->detailsTable, { url.toString(), std::move(error) });
    m_ui->detailsTable->addTopLevelItem(item);
    if (m_ui->detailsTable->selectedItems().isEmpty()) {
        m_ui->detailsTable->setCurrentItem(item);
    }
}

void NetworkJobFailedDialog::showPermissionRepairOptions() const
{
    if (m_permissionHint)
        m_permissionHint->setVisible(true);
    if (m_stagingButton)
        m_stagingButton->setVisible(true);
    if (m_avGuideButton)
        m_avGuideButton->setVisible(true);
}

void NetworkJobFailedDialog::copyUrl() const
{
    auto items = m_ui->detailsTable->selectedItems();
    if (items.isEmpty()) {
        return;
    }

    auto* clipboard = QGuiApplication::clipboard();
    clipboard->setText(items.first()->text(0));
}

void NetworkJobFailedDialog::reportBug()
{
    QStringList lines;
    for (int i = 0; i < m_ui->detailsTable->topLevelItemCount(); i++) {
        const auto* item = m_ui->detailsTable->topLevelItem(i);
        lines << QStringLiteral("%1 - %2").arg(item->text(0), item->text(1));
    }
    // The dialog stays open: the user can still Retry or Abort after reporting.
    BugReportDialog::reportError(this, tr("Network error: %1").arg(m_jobName), lines.join('\n'));
}

void NetworkJobFailedDialog::retryWithDifferentTempFolder()
{
    const QString current = APPLICATION->downloadStagingDir();
    const QString dir = QFileDialog::getExistingDirectory(
        this, tr("Pick a temporary download folder"),
        current.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::TempLocation) : current);
    if (dir.isEmpty()) {
        return;  // changed their mind; the error window stays open
    }

    qInfo() << "Downloads will be staged in" << dir << "(antivirus workaround)";
    APPLICATION->setDownloadStagingDir(dir);

    // accept() is the Retry path of the NetJob that opened this window: the failed
    // requests run again, now downloading into the folder chosen above.
    accept();
}

void NetworkJobFailedDialog::openAntivirusGuide()
{
    if (BuildConfig.AV_EXCLUSION_URL.isEmpty())
        return;
    DesktopServices::openUrl(QUrl(BuildConfig.AV_EXCLUSION_URL));
}
