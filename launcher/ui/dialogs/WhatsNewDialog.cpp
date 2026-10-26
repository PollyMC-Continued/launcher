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

#include "WhatsNewDialog.h"

#include <QCheckBox>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QHBoxLayout>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QTextBrowser>
#include <QUrl>
#include <QVBoxLayout>

#include <DesktopServices.h>

#include "Application.h"
#include "BuildConfig.h"
#include "FileSystem.h"
#include "Markdown.h"
#include "StringUtils.h"
#include "ui/GuiUtil.h"
#include "updater/UpdaterNames.h"
#include "settings/SettingsObject.h"

namespace {
QString storedNotesPath()
{
    return QDir(APPLICATION->dataRoot()).filePath(QStringLiteral("whatsnew.json"));
}
}  // namespace

WhatsNewDialog::WhatsNewDialog(const QString& version, const QString& previousVersion, const QString& notesMarkdown, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("What's New"));
    setModal(true);
    resize(640, 520);

    auto* root = new QVBoxLayout(this);

    // ---- header: icon, title, "from -> to" ----
    auto* header = new QHBoxLayout;
    auto* icon = new QLabel(this);
    icon->setPixmap(QIcon::fromTheme("checkupdate").pixmap(48));
    icon->setAlignment(Qt::AlignTop);
    header->addWidget(icon);

    auto* titles = new QVBoxLayout;
    auto* title = new QLabel(version.isEmpty() ? tr("What's new in %1").arg(BuildConfig.LAUNCHER_DISPLAYNAME)
                                               : tr("What's new in %1 %2").arg(BuildConfig.LAUNCHER_DISPLAYNAME, version),
                             this);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    if (titleFont.pointSizeF() > 0)
        titleFont.setPointSizeF(titleFont.pointSizeF() * 1.4);
    title->setFont(titleFont);
    titles->addWidget(title);

    if (!previousVersion.isEmpty() && !version.isEmpty()) {
        auto* sub = new QLabel(tr("You updated from %1 to %2.").arg(previousVersion, version), this);
        sub->setEnabled(false);  // dimmed, like a caption
        titles->addWidget(sub);
    }
    header->addLayout(titles, 1);
    root->addLayout(header);

    // ---- the release notes ----
    auto* notes = new QTextBrowser(this);
    notes->setOpenExternalLinks(true);
    notes->setLineWrapMode(QTextBrowser::LineWrapMode::WidgetWidth);
    notes->setVerticalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAsNeeded);
    if (notesMarkdown.trimmed().isEmpty()) {
        notes->setHtml(QStringLiteral("<p>%1</p>")
                           .arg(tr("The release notes for this version are not stored on this computer. "
                                   "Use \"All releases\" to read them on GitHub.")
                                    .toHtmlEscaped()));
    } else {
        notes->setHtml(StringUtils::htmlListPatch(markdownToHTML(notesMarkdown)));
    }
    root->addWidget(notes, 1);

    // ---- footer: opt-out on the left, buttons on the right ----
    auto* footer = new QHBoxLayout;
    auto* showAgain = new QCheckBox(tr("Show this after updates"), this);
    showAgain->setChecked(APPLICATION->settings()->get("WhatsNewEnabled").toBool());
    showAgain->setToolTip(tr("You can always open this window again from Help > What's New."));
    connect(showAgain, &QCheckBox::toggled, this, [](bool checked) { APPLICATION->settings()->set("WhatsNewEnabled", checked); });
    footer->addWidget(showAgain);
    footer->addStretch(1);

    auto* releases = new QPushButton(tr("All releases"), this);
    releases->setAutoDefault(false);
    connect(releases, &QPushButton::clicked, this, [] { DesktopServices::openUrl(QUrl(WhatsNewDialog::releasesUrl())); });
    footer->addWidget(releases);

    auto* close = new QPushButton(tr("Got it"), this);
    close->setDefault(true);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    footer->addWidget(close);
    root->addLayout(footer);

    // remember the window size between runs, like the update dialog does
    GuiUtil::rememberGeometry(this, "WhatsNewDialogGeometry");
}

QString WhatsNewDialog::releasesUrl()
{
    QString base = BuildConfig.UPDATER_GITHUB_REPO;
    while (base.endsWith('/'))
        base.chop(1);
    return base.isEmpty() ? QString() : base + QStringLiteral("/releases");
}

QString WhatsNewDialog::takePendingNotes()
{
    const QString dir = APPLICATION->dataRoot();
    // older releases wrote the legacy name; resolve() picks whichever exists
    const QString path = UpdaterNames::resolve(dir, UpdaterNames::ChangelogFile);
    QString notes;
    if (QFileInfo::exists(path))
        notes = QString::fromUtf8(FS::read(path));
    UpdaterNames::removeAll(dir, UpdaterNames::ChangelogFile);
    return notes.trimmed();
}

void WhatsNewDialog::discardPendingNotes()
{
    UpdaterNames::removeAll(APPLICATION->dataRoot(), UpdaterNames::ChangelogFile);
}

void WhatsNewDialog::storeNotes(const QString& version, const QString& markdown)
{
    QJsonObject obj;
    obj.insert("version", version);
    obj.insert("notes", markdown);
    try {
        FS::write(storedNotesPath(), QJsonDocument(obj).toJson(QJsonDocument::Compact));
    } catch (...) {
        qWarning() << "Could not store the release notes for the What's New window";
    }
}

WhatsNewDialog::Notes WhatsNewDialog::loadStoredNotes()
{
    Notes result;
    const QString path = storedNotesPath();
    if (!QFileInfo::exists(path))
        return result;
    const auto doc = QJsonDocument::fromJson(FS::read(path));
    if (!doc.isObject())
        return result;
    result.version = doc.object().value("version").toString();
    result.markdown = doc.object().value("notes").toString();
    return result;
}
