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

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPalette>
#include <QPushButton>
#include <QScrollArea>
#include <QTime>
#include <QTimer>
#include <QVBoxLayout>

#include "BugReportTracker.h"

namespace {

// Mid-tone colours that stay readable with white text on both light and dark themes.
QString statusColor(const QString& status)
{
    if (status == QLatin1String("fixed") || status == QLatin1String("fixed_next"))
        return QStringLiteral("#2e9e5b");  // green
    if (status == QLatin1String("need_info"))
        return QStringLiteral("#c77c0e");  // amber: the user has to do something
    if (status == QLatin1String("seen"))
        return QStringLiteral("#3b7dd8");  // blue
    if (status == QLatin1String("wont_fix") || status == QLatin1String("not_a_bug") || status == QLatin1String("duplicate"))
        return QStringLiteral("#7a828e");  // grey
    return QStringLiteral("#7a828e");      // received / unknown
}

// Short text for the coloured pill; the long text goes under the title.
QString pillText(const QString& status)
{
    if (status == QLatin1String("received"))
        return QObject::tr("Waiting");
    if (status == QLatin1String("seen"))
        return QObject::tr("In review");
    if (status == QLatin1String("need_info"))
        return QObject::tr("Needs info");
    if (status == QLatin1String("fixed_next") || status == QLatin1String("fixed"))
        return QObject::tr("Fixed");
    if (status == QLatin1String("wont_fix"))
        return QObject::tr("Won't be fixed");
    if (status == QLatin1String("duplicate"))
        return QObject::tr("Duplicate");
    if (status == QLatin1String("not_a_bug"))
        return QObject::tr("Not a bug");
    return QObject::tr("Updated");
}

QString dateText(qint64 secs)
{
    return QLocale().toString(QDateTime::fromSecsSinceEpoch(secs).date(), QLocale::ShortFormat);
}

QWidget* makeCard(const BugReportTracker::Entry& e, bool isNew, QWidget* parent)
{
    const QPalette pal = parent->palette();
    auto* card = new QFrame(parent);
    card->setObjectName(QStringLiteral("reportCard"));
    card->setStyleSheet(QStringLiteral("#reportCard { background-color: %1; border: 1px solid %2; border-radius: 8px; }")
                            .arg(pal.color(QPalette::Base).name(), pal.color(QPalette::Mid).name()));

    auto* outer = new QVBoxLayout(card);
    outer->setContentsMargins(14, 12, 14, 12);
    outer->setSpacing(6);

    // row 1: status pill, "new" tag, date on the right
    auto* top = new QHBoxLayout();
    top->setSpacing(8);
    auto* pill = new QLabel(pillText(e.status), card);
    pill->setStyleSheet(QStringLiteral("background-color: %1; color: white; font-weight: 600; border-radius: 9px; padding: 2px 10px;")
                            .arg(statusColor(e.status)));
    top->addWidget(pill);
    if (isNew) {
        auto* tag = new QLabel(QObject::tr("NEW"), card);
        tag->setStyleSheet(QStringLiteral("color: %1; font-weight: 700;").arg(pal.color(QPalette::Highlight).name()));
        top->addWidget(tag);
    }
    top->addStretch(1);
    auto* date = new QLabel(dateText(e.sent), card);
    date->setEnabled(false);  // greyed out by the theme
    date->setToolTip(QObject::tr("Sent"));
    top->addWidget(date);
    outer->addLayout(top);

    // row 2: what the user wrote
    auto* title = new QLabel(e.title.isEmpty() ? QObject::tr("(no title)") : e.title, card);
    QFont f = title->font();
    f.setBold(true);
    f.setPointSizeF(f.pointSizeF() * 1.1);
    title->setFont(f);
    title->setWordWrap(true);
    outer->addWidget(title);

    // row 3: the team's answer in words
    auto* answer = new QLabel(BugReportTracker::statusText(e.status, e.version), card);
    answer->setWordWrap(true);
    outer->addWidget(answer);

    // row 4: type, ID and a copy button
    auto* bottom = new QHBoxLayout();
    bottom->setSpacing(8);
    auto* meta = new QLabel(QObject::tr("%1  ·  ID %2").arg(BugReportTracker::typeText(e.type), BugReportTracker::displayId(e.id)), card);
    meta->setEnabled(false);
    meta->setTextInteractionFlags(Qt::TextSelectableByMouse);
    bottom->addWidget(meta);
    bottom->addStretch(1);
    auto* copy = new QPushButton(QObject::tr("Copy ID"), card);
    copy->setToolTip(QObject::tr("Give this ID to the team if you contact them about this report."));
    copy->setFlat(true);
    copy->setCursor(Qt::PointingHandCursor);
    const QString id = BugReportTracker::displayId(e.id);
    QObject::connect(copy, &QPushButton::clicked, copy, [copy, id]() {
        QGuiApplication::clipboard()->setText(id);
        copy->setText(QObject::tr("Copied!"));
        QTimer::singleShot(1500, copy, [copy]() { copy->setText(QObject::tr("Copy ID")); });
    });
    bottom->addWidget(copy);
    outer->addLayout(bottom);

    return card;
}

}  // namespace

MyReportsDialog::MyReportsDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("My Reports"));
    setMinimumSize(480, 360);
    resize(620, 520);

    auto* heading = new QLabel(tr("My Reports"), this);
    QFont hf = heading->font();
    hf.setBold(true);
    hf.setPointSizeF(hf.pointSizeF() * 1.4);
    heading->setFont(hf);

    auto* intro = new QLabel(tr("Bug reports and suggestions you sent from this launcher, and what the team answered."), this);
    intro->setWordWrap(true);
    intro->setEnabled(false);

    // list page: cards in a scroll area
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* inner = new QWidget(scroll);
    m_cards = new QVBoxLayout(inner);
    m_cards->setContentsMargins(0, 0, 8, 0);  // room for the scroll bar
    m_cards->setSpacing(10);
    scroll->setWidget(inner);
    m_listPage = scroll;

    // empty page
    m_emptyPage = new QWidget(this);
    auto* emptyLayout = new QVBoxLayout(m_emptyPage);
    emptyLayout->addStretch(1);
    auto* emptyTitle = new QLabel(tr("No reports yet"), m_emptyPage);
    QFont ef = emptyTitle->font();
    ef.setBold(true);
    ef.setPointSizeF(ef.pointSizeF() * 1.2);
    emptyTitle->setFont(ef);
    emptyTitle->setAlignment(Qt::AlignCenter);
    auto* emptyText = new QLabel(tr("When you send a bug report or suggestion from this launcher, it shows up here, and you will be told when the team answers."), m_emptyPage);
    emptyText->setAlignment(Qt::AlignCenter);
    emptyText->setWordWrap(true);
    emptyText->setEnabled(false);
    emptyLayout->addWidget(emptyTitle);
    emptyLayout->addWidget(emptyText);
    emptyLayout->addStretch(1);

    // footer
    m_checked = new QLabel(this);
    m_checked->setEnabled(false);
    m_refresh = new QPushButton(tr("Check for updates"), this);
    auto* close = new QPushButton(tr("Close"), this);
    close->setDefault(true);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_refresh, &QPushButton::clicked, this, &MyReportsDialog::refresh);

    auto* footer = new QHBoxLayout();
    footer->addWidget(m_checked, 1);
    footer->addWidget(m_refresh);
    footer->addWidget(close);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(8);
    layout->addWidget(heading);
    layout->addWidget(intro);
    layout->addSpacing(4);
    layout->addWidget(m_listPage, 1);
    layout->addWidget(m_emptyPage, 1);
    layout->addLayout(footer);

    m_checker = new BugReportStatusChecker(this);
    connect(m_checker, &BugReportStatusChecker::finished, this, [this](const QList<BugReportTracker::Entry>& unseen) {
        for (const auto& e : unseen)
            m_newIds.insert(e.id);
        m_refresh->setEnabled(true);
        m_refresh->setText(tr("Check for updates"));
        m_checked->setText(tr("Last checked: %1").arg(QLocale().toString(QTime::currentTime(), QLocale::ShortFormat)));
        BugReportTracker::markAnnounced();  // the user is looking at them right now
        reload();
    });

    // Remember which reports have news the user has not seen, so the cards can say NEW, then count them all as seen.
    for (const auto& e : BugReportTracker::load())
        if (e.status != e.announced)
            m_newIds.insert(e.id);
    BugReportTracker::markAnnounced();
    reload();
}

void MyReportsDialog::reload()
{
    while (auto* item = m_cards->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    const auto list = BugReportTracker::load();
    for (const auto& e : list)
        m_cards->addWidget(makeCard(e, m_newIds.contains(e.id), m_listPage));
    m_cards->addStretch(1);

    m_listPage->setVisible(!list.isEmpty());
    m_emptyPage->setVisible(list.isEmpty());
}

void MyReportsDialog::refresh()
{
    m_refresh->setEnabled(false);
    m_refresh->setText(tr("Checking…"));
    m_checker->check();
}
