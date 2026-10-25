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

#include "InstanceHealthCheckDialog.h"

#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

#include "Application.h"
#include "FileSystem.h"
#include "MMCZip.h"
#include "java/JavaVersion.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "settings/SettingsObject.h"
#include "ui/GuiUtil.h"

namespace {
struct HealthIssue {
    ProblemSeverity severity = ProblemSeverity::None;
    QString title;
    QString detail;
};

QIcon severityIcon(ProblemSeverity severity)
{
    switch (severity) {
        case ProblemSeverity::Error:
            return QIcon::fromTheme(QStringLiteral("status-bad"));
        case ProblemSeverity::Warning:
            return QIcon::fromTheme(QStringLiteral("status-yellow"));
        default:
            return QIcon::fromTheme(QStringLiteral("status-good"));
    }
}

// ---- Java: does it exist, and does its version fit this instance? ----
void checkJava(MinecraftInstance* instance, QList<HealthIssue>& issues)
{
    auto settings = instance->settings();
    const QString javaPath = settings->get("JavaPath").toString();
    if (javaPath.isEmpty()) {
        issues.append({ ProblemSeverity::Error, InstanceHealthCheckDialog::tr("No Java selected"),
                        InstanceHealthCheckDialog::tr("This instance has no Java installation selected, so it cannot start.\n"
                                                      "Pick one in the instance's Java settings, or let the launcher download "
                                                      "a matching Java automatically.") });
        return;
    }

    const QString resolved = FS::ResolveExecutable(javaPath);
    if (resolved.isNull()) {
        issues.append({ ProblemSeverity::Error, InstanceHealthCheckDialog::tr("Java not found"),
                        InstanceHealthCheckDialog::tr("The Java installation for this instance does not exist or cannot be run:\n%1\n"
                                                      "It may have been moved, uninstalled, or blocked by antivirus software.")
                            .arg(javaPath) });
        return;
    }

    // Version compatibility - only when we know both the Java version and what the instance needs.
    const QString javaVersion = settings->get("JavaVersion").toString();
    const auto compatibleMajors = instance->getPackProfile()->getProfile()->getCompatibleJavaMajors();
    const bool ignoreCompatibility = settings->get("IgnoreJavaCompatibility").toBool();
    if (!javaVersion.isEmpty() && !compatibleMajors.isEmpty()) {
        JavaVersion version(javaVersion);
        if (!compatibleMajors.contains(version.major())) {
            QStringList wanted;
            for (int major : compatibleMajors)
                wanted << QString::number(major);
            const QString detail = InstanceHealthCheckDialog::tr("This instance needs Java %1, but the selected Java is version %2.\n"
                                                                  "Switch Java in the instance's Java settings, or turn off the "
                                                                  "compatibility check if you know what you're doing.")
                                       .arg(wanted.join(QStringLiteral(", ")), javaVersion);
            issues.append({ ignoreCompatibility ? ProblemSeverity::Warning : ProblemSeverity::Error,
                            InstanceHealthCheckDialog::tr("Java version mismatch"), detail });
        }
    }
}

// ---- Mod jars: a jar that no longer opens as an archive will crash the game at startup ----
void checkModJars(MinecraftInstance* instance, QList<HealthIssue>& issues)
{
    const QDir modsDir(FS::PathCombine(instance->gameRoot(), "mods"));
    if (!modsDir.exists())
        return;

    QStringList broken;
    const auto jars = modsDir.entryInfoList({ QStringLiteral("*.jar") }, QDir::Files);
    for (const QFileInfo& jar : jars) {
        if (jar.size() == 0) {
            broken << jar.fileName();
            continue;
        }
        MMCZip::ArchiveReader zip(jar.absoluteFilePath());
        bool hasEntries = false;
        const bool opens = zip.parse([&hasEntries](MMCZip::ArchiveReader::File*, bool& stop) {
            hasEntries = true;
            stop = true;  // one entry is enough to prove the archive opens
            return true;
        });
        if (!opens || !hasEntries)
            broken << jar.fileName();
    }

    if (!broken.isEmpty()) {
        const int shown = 8;
        QString names = broken.mid(0, shown).join(QStringLiteral("\n"));
        if (broken.size() > shown)
            names += QStringLiteral("\n…");
        issues.append({ ProblemSeverity::Error, InstanceHealthCheckDialog::tr("Broken mod files (%1)").arg(broken.size()),
                        InstanceHealthCheckDialog::tr("These files in the mods folder are empty or damaged, and usually make the game "
                                                      "crash on startup:\n%1\n\nDelete or re-download them (a fresh download of the "
                                                      "same version is usually enough).")
                            .arg(names) });
    }
}

// ---- Loader and components: the version page knows what is broken or missing ----
void checkComponents(MinecraftInstance* instance, QList<HealthIssue>& issues)
{
    auto profile = instance->getPackProfile();
    const int count = profile->rowCount();

    // an empty component list means the instance was never fully loaded/resolved
    if (count == 0 || profile->getComponentVersion(QStringLiteral("net.minecraft")).isEmpty()) {
        issues.append({ ProblemSeverity::Warning, InstanceHealthCheckDialog::tr("Version information not loaded yet"),
                        InstanceHealthCheckDialog::tr("The launcher could not read this instance's version list, so the loader check "
                                                      "was skipped.\nOpen the instance, go to the Version page and let it load once "
                                                      "(or press Reload) before launching.") });
        return;
    }

    for (int i = 0; i < count; ++i) {
        auto component = profile->getComponent(static_cast<size_t>(i));
        if (!component)
            continue;
        const auto problems = component->getProblems();
        for (const auto& problem : problems) {
            issues.append({ problem.m_severity,
                            InstanceHealthCheckDialog::tr("%1 (loader component)").arg(component->getName()),
                            problem.m_description });
        }
    }
}
}  // namespace

InstanceHealthCheckDialog::InstanceHealthCheckDialog(MinecraftInstance* instance, QWidget* parent)
    : QDialog(parent), m_instance(instance)
{
    setWindowTitle(tr("Check instance"));
    setWindowModality(Qt::WindowModal);

    auto* layout = new QVBoxLayout(this);

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setWordWrap(true);
    layout->addWidget(m_summaryLabel);

    m_issuesList = new QListWidget(this);
    m_issuesList->setWordWrap(true);
    m_issuesList->setAlternatingRowColors(true);
    m_issuesList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    layout->addWidget(m_issuesList, 1);

    m_closeButton = new QPushButton(tr("Close"), this);
    m_closeButton->setDefault(true);
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);
    auto* buttonRow = new QHBoxLayout();
    buttonRow->addStretch(1);
    buttonRow->addWidget(m_closeButton);
    layout->addLayout(buttonRow);

    runChecks();

    // remember the window size between runs, like the settings window does
    GuiUtil::rememberGeometry(this, "InstanceCheckGeometry");
}

void InstanceHealthCheckDialog::runChecks()
{
    QList<HealthIssue> issues;

    checkJava(m_instance, issues);
    checkModJars(m_instance, issues);
    checkComponents(m_instance, issues);

    if (issues.isEmpty()) {
        m_summaryLabel->setText(tr("<b>%1</b> looks good - Java was found, no broken mod files, and the loader version checks out. "
                                   "Nothing stood in the way of launching.")
                                    .arg(m_instance->name().toHtmlEscaped()));
        m_issuesList->hide();
    } else {
        m_summaryLabel->setText(tr("<b>%1</b> has %n problem(s) that can stop the game from starting. The details are below each entry.",
                                   "", issues.size())
                                    .arg(m_instance->name().toHtmlEscaped()));
        for (const auto& issue : issues) {
            // QListWidget items are plain text (no HTML), and constructing with a parent already adds the item
            const QString text = QStringLiteral("%1\n%2").arg(issue.title, issue.detail);
            auto* item = new QListWidgetItem(severityIcon(issue.severity), text, m_issuesList);
            item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        }
    }

    // a sensible default that never runs off the screen; restoreGeometry (in the constructor) may override it
    const QSize available = screen() ? screen()->availableGeometry().size() : QSize(1280, 720);
    const QSize maxSize(available.width() * 9 / 10, available.height() * 9 / 10);
    resize(QSize(560, 520).boundedTo(maxSize));
}
