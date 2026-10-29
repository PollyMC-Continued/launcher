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

#include "MyReportsDialog.h"

#include <QDateTime>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "BugReportTracker.h"

MyReportsDialog::MyReportsDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("My Reports"));
    resize(720, 380);

    auto* intro = new QLabel(tr("Bug reports and suggestions sent from this launcher, and what the team answered."), this);
    intro->setWordWrap(true);

    m_tree = new QTreeWidget(this);
    m_tree->setRootIsDecorated(false);
    m_tree->setAlternatingRowColors(true);
    m_tree->setHeaderLabels({ tr("Report ID"), tr("Type"), tr("What you wrote"), tr("Status"), tr("Sent") });
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);

    m_empty = new QLabel(tr("You have not sent any reports from this launcher yet."), this);
    m_empty->setAlignment(Qt::AlignCenter);

    m_refresh = new QPushButton(tr("Check for updates"), this);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->addButton(m_refresh, QDialogButtonBox::ActionRole);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_refresh, &QPushButton::clicked, this, &MyReportsDialog::refresh);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(intro);
    layout->addWidget(m_tree, 1);
    layout->addWidget(m_empty, 1);
    layout->addWidget(buttons);

    m_checker = new BugReportStatusChecker(this);
    connect(m_checker, &BugReportStatusChecker::finished, this, [this]() {
        m_refresh->setEnabled(true);
        m_refresh->setText(tr("Check for updates"));
        BugReportTracker::markAnnounced();  // the user is looking at them right now
        reload();
    });

    BugReportTracker::markAnnounced();  // opening this window counts as having seen every update
    reload();
}

void MyReportsDialog::reload()
{
    m_tree->clear();
    const auto list = BugReportTracker::load();
    for (const auto& e : list) {
        auto* item = new QTreeWidgetItem(m_tree);
        item->setText(0, BugReportTracker::displayId(e.id));
        item->setText(1, BugReportTracker::typeText(e.type));
        item->setText(2, e.title);
        item->setText(3, BugReportTracker::statusText(e.status, e.version));
        item->setText(4, QLocale().toString(QDateTime::fromSecsSinceEpoch(e.sent).date(), QLocale::ShortFormat));
        item->setToolTip(0, tr("Give this ID to the team if you contact them about this report."));
        item->setToolTip(3, BugReportTracker::statusText(e.status, e.version));
    }
    m_tree->setVisible(!list.isEmpty());
    m_empty->setVisible(list.isEmpty());
}

void MyReportsDialog::refresh()
{
    m_refresh->setEnabled(false);
    m_refresh->setText(tr("Checking…"));
    m_checker->check();
}
