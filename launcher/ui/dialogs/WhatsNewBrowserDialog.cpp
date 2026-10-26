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

#include "WhatsNewBrowserDialog.h"

#include <QFile>
#include <QFont>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QRegularExpression>
#include <QSplitter>
#include <QTextBrowser>
#include <QUrl>
#include <QVBoxLayout>

#include <DesktopServices.h>

#include "BuildConfig.h"
#include "Markdown.h"
#include "StringUtils.h"
#include "ui/GuiUtil.h"
#include "ui/dialogs/WhatsNewDialog.h"

namespace {
/// "v9.5.0" and "9.5.0-beta" both become "9.5.0", so the changelog's leading "v" and a channel
/// suffix do not keep a release from matching the version that is currently running.
QString normalizeVersion(QString version)
{
    if (version.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
        version.remove(0, 1);
    const int channel = version.indexOf(QLatin1Char('-'));
    if (channel >= 0)
        version.truncate(channel);
    return version.trimmed();
}
}  // namespace

WhatsNewBrowserDialog::WhatsNewBrowserDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("What's New"));
    resize(860, 560);

    auto* root = new QVBoxLayout(this);

    // ---- header: icon and the title of the selected release ----
    auto* header = new QHBoxLayout;
    auto* icon = new QLabel(this);
    icon->setPixmap(QIcon::fromTheme("checkupdate").pixmap(48));
    icon->setAlignment(Qt::AlignTop);
    header->addWidget(icon);

    m_titleLabel = new QLabel(this);
    QFont titleFont = m_titleLabel->font();
    titleFont.setBold(true);
    if (titleFont.pointSizeF() > 0)
        titleFont.setPointSizeF(titleFont.pointSizeF() * 1.4);
    m_titleLabel->setFont(titleFont);
    header->addWidget(m_titleLabel, 1);
    root->addLayout(header);

    // ---- sidebar with every version, next to the notes of the selected one ----
    m_releases = loadReleases();

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);

    m_versionList = new QListWidget(splitter);
    m_notesBrowser = new QTextBrowser(splitter);
    m_notesBrowser->setOpenExternalLinks(true);
    m_notesBrowser->setLineWrapMode(QTextBrowser::LineWrapMode::WidgetWidth);
    m_notesBrowser->setVerticalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAsNeeded);

    splitter->addWidget(m_versionList);
    splitter->addWidget(m_notesBrowser);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes(QList<int>{ 150, 710 });
    root->addWidget(splitter, 1);

    // ---- footer: the releases page on the left, close on the right ----
    auto* footer = new QHBoxLayout;
    auto* releases = new QPushButton(tr("All releases"), this);
    releases->setAutoDefault(false);
    connect(releases, &QPushButton::clicked, this,
            [] { DesktopServices::openUrl(QUrl(WhatsNewDialog::releasesUrl())); });
    footer->addWidget(releases);
    footer->addStretch(1);

    auto* close = new QPushButton(tr("Close"), this);
    close->setDefault(true);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    footer->addWidget(close);
    root->addLayout(footer);

    connect(m_versionList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0 && row < m_releases.size())
            showRelease(m_releases.at(row));
    });

    if (m_releases.isEmpty()) {
        m_titleLabel->setText(tr("What's new in %1").arg(BuildConfig.LAUNCHER_DISPLAYNAME));
        m_notesBrowser->setHtml(QStringLiteral("<p>%1</p>")
                                    .arg(tr("The changelog could not be read. Use \"All releases\" to read the "
                                            "release notes on GitHub.")
                                             .toHtmlEscaped()));
    } else {
        const QString running = normalizeVersion(BuildConfig.printableVersionString());
        int initialRow = 0;  // the newest release
        for (int i = 0; i < m_releases.size(); ++i) {
            auto* item = new QListWidgetItem(m_releases.at(i).version, m_versionList);
            if (normalizeVersion(m_releases.at(i).version) == running) {
                initialRow = i;
                auto font = item->font();
                font.setBold(true);
                item->setFont(font);
                item->setToolTip(tr("The version this launcher is running."));
            }
        }
        // fills the title and the notes through currentRowChanged
        m_versionList->setCurrentRow(initialRow);
    }

    // remember the window size between runs, like the other windows do
    GuiUtil::rememberGeometry(this, "WhatsNewBrowserGeometry");
}

void WhatsNewBrowserDialog::showRelease(const Release& release)
{
    m_titleLabel->setText(tr("What's new in %1 %2").arg(BuildConfig.LAUNCHER_DISPLAYNAME, release.version));
    m_notesBrowser->setHtml(StringUtils::htmlListPatch(markdownToHTML(release.markdown)));
}

QList<WhatsNewBrowserDialog::Release> WhatsNewBrowserDialog::loadReleases()
{
    // Version sections look like "## v9.5.0"; other "##" headings (if any) are kept as content.
    static const QRegularExpression versionHeading(QStringLiteral("^##\\s+((?:v)?\\d[\\w.+-]*)\\s*$"));

    QList<Release> releases;
    QFile file(QStringLiteral(":/documents/CHANGELOG.md"));
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open" << file.fileName() << "for reading:" << file.errorString();
        return releases;
    }
    const QStringList lines = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'));

    for (const QString& line : lines) {
        const auto match = versionHeading.match(line);
        if (match.hasMatch()) {
            releases.append(Release{ match.captured(1), QString() });
        } else if (!releases.isEmpty()) {
            // Lines before the first version heading (the "# Changelog" title) are skipped.
            releases.last().markdown += line;
            releases.last().markdown += QLatin1Char('\n');
        }
    }
    for (auto& release : releases)
        release.markdown = release.markdown.trimmed();
    return releases;
}
