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

#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLabel>
#include <QListWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QSet>
#include <QStorageInfo>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent>
#include <algorithm>

#include "Application.h"
#include "DesktopServices.h"
#include "FileSystem.h"
#include "HardwareInfo.h"
#include "InstanceCreationTask.h"
#include "MMCZip.h"
#include "java/JavaVersion.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "minecraft/mod/Mod.h"
#include "minecraft/mod/ModConflictChecker.h"
#include "minecraft/mod/ModFolderModel.h"
#include "settings/SettingsObject.h"
#include "ui/GuiUtil.h"

using Issue = InstanceHealthCheckDialog::Issue;

namespace {
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

QString severityWord(ProblemSeverity severity)
{
    switch (severity) {
        case ProblemSeverity::Error:
            return InstanceHealthCheckDialog::tr("Error");
        case ProblemSeverity::Warning:
            return InstanceHealthCheckDialog::tr("Warning");
        default:
            return InstanceHealthCheckDialog::tr("Info");
    }
}

// "a.jar, b.jar and 12 more" without running off the screen
QString listNames(const QStringList& names, int shown = 8)
{
    QString text = names.mid(0, shown).join(QStringLiteral("\n"));
    if (names.size() > shown)
        text += InstanceHealthCheckDialog::tr("\n… and %n more", "", names.size() - shown);
    return text;
}

// ---- Java: does it exist, and does its version fit this instance? ----
void checkJava(MinecraftInstance* instance, QList<Issue>& issues)
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

// ---- Loader and components: the version page knows what is broken or missing ----
void checkComponents(MinecraftInstance* instance, QList<Issue>& issues)
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
            issues.append({ problem.m_severity, InstanceHealthCheckDialog::tr("%1 (loader component)").arg(component->getName()),
                            problem.m_description });
        }
    }
}

// ---- Disk: a launch writes logs, libraries and sometimes a crash dump ----
void checkDiskSpace(MinecraftInstance* instance, QList<Issue>& issues)
{
    const QStorageInfo storage(instance->gameRoot());
    if (!storage.isValid() || !storage.isReady())
        return;

    const qint64 freeMiB = storage.bytesAvailable() / (1024 * 1024);
    if (freeMiB >= 1024)
        return;

    const QString detail = InstanceHealthCheckDialog::tr("Only %1 MiB are free on the drive this instance is stored on.\n"
                                                         "Free up some space, or the game can fail while saving worlds or downloading files.")
                               .arg(freeMiB);
    issues.append({ freeMiB < 100 ? ProblemSeverity::Error : ProblemSeverity::Warning, InstanceHealthCheckDialog::tr("Low disk space"),
                    detail });
}

// ---- Memory: asking for more than the computer has makes the JVM fail to start, or swap until the game is unusable ----
void checkMemory(MinecraftInstance* instance, QList<Issue>& issues)
{
    const quint64 totalMiB = HardwareInfo::totalRamMiB();
    const quint64 maxMiB = instance->settings()->get("MaxMemAlloc").toUInt();
    if (totalMiB == 0 || maxMiB == 0)
        return;  // unknown

    if (maxMiB > totalMiB) {
        issues.append({ ProblemSeverity::Error, InstanceHealthCheckDialog::tr("Too much memory allocated"),
                        InstanceHealthCheckDialog::tr("This instance is set to use up to %1 MiB, but the computer only has %2 MiB.\n"
                                                      "Lower the maximum memory in the instance's settings.")
                            .arg(maxMiB)
                            .arg(totalMiB) });
    } else if (maxMiB * 100 > totalMiB * 80) {
        issues.append({ ProblemSeverity::Warning, InstanceHealthCheckDialog::tr("A lot of memory allocated"),
                        InstanceHealthCheckDialog::tr("This instance may use up to %1 MiB of the computer's %2 MiB.\n"
                                                      "That leaves little for the system and can make the game stutter; "
                                                      "around half of the total is usually plenty.")
                            .arg(maxMiB)
                            .arg(totalMiB) });
    }
}

// ---- Mod jars: a jar that no longer opens as an archive will crash the game at startup ----
// Runs in a worker thread, so it only gets a path and touches nothing else.
QStringList scanBrokenJars(const QString& modsPath)
{
    QStringList broken;
    const QDir modsDir(modsPath);
    if (!modsDir.exists())
        return broken;

    const auto jars = modsDir.entryInfoList({ QStringLiteral("*.jar") }, QDir::Files, QDir::Name);
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
    return broken;
}

// ---- Pack files: a managed instance should still contain what its pack index says ----
// Compares the mods folder against the pack's own file list: files of the pack that are gone,
// and jars the pack does not know about (added by hand, or left behind by a pack update).
// CurseForge manifests do not carry file names, so those cannot be checked offline (yet).
void checkManagedPackFiles(MinecraftInstance* instance, QList<Issue>& issues)
{
    if (!instance->isManagedPack() || instance->getManagedPackType() != QLatin1String("modrinth"))
        return;

    const QString indexPath = FS::PathCombine(FS::PathCombine(instance->instanceRoot(), "mrpack"), "modrinth.index.json");
    QFileInfo indexInfo(indexPath);
    if (!indexInfo.exists() || indexInfo.size() == 0)
        return;  // no index to compare against (e.g. copied instance)

    QFile indexFile(indexPath);
    if (!indexFile.open(QFile::ReadOnly))
        return;

    QJsonParseError jsonError{};
    const auto doc = QJsonDocument::fromJson(indexFile.readAll(), &jsonError);
    if (jsonError.error != QJsonParseError::NoError || !doc.isObject())
        return;

    const QDir gameDir(instance->gameRoot());
    const QString packLabel =
        QStringLiteral("%1 %2").arg(instance->getManagedPackName(), instance->getManagedPackVersionName()).trimmed();

    QStringList missing;
    QSet<QString> packModFiles;
    const auto files = doc.object().value(QStringLiteral("files")).toArray();
    for (const auto& fileValue : files) {
        const auto file = fileValue.toObject();
        QString path = file.value(QStringLiteral("path")).toString().replace(QLatin1Char('\\'), QLatin1Char('/'));
        if (path.isEmpty())
            continue;
        // files the pack explicitly does not want on the client
        if (file.value(QStringLiteral("env")).toObject().value(QStringLiteral("client")).toString() ==
            QLatin1String("unsupported"))
            continue;

        if (path.startsWith(QStringLiteral("mods/")))
            packModFiles.insert(path);

        if (!InstanceCreationTask::packFileExists(gameDir, path))
            missing << path;
    }

    if (!missing.isEmpty()) {
        std::sort(missing.begin(), missing.end());
        issues.append({ ProblemSeverity::Warning,
                        InstanceHealthCheckDialog::tr("Pack files missing (%1)").arg(missing.size()),
                        InstanceHealthCheckDialog::tr("These files belong to %1, but they are not in the instance anymore:\n%2\n\n"
                                                      "Running Update Pack downloads them again. If you removed any of them on purpose, "
                                                      "disable them instead of deleting, so the launcher can tell the difference.")
                            .arg(packLabel, listNames(missing)) });
    }

    // jars in the mods folder that no pack version ever listed: user-added, or leftovers from
    // an update where the pack replaced a mod under a different file name
    QStringList orphans;
    const QDir modsDir(FS::PathCombine(instance->gameRoot(), "mods"));
    const auto jars = modsDir.entryInfoList({ QStringLiteral("*.jar") }, QDir::Files, QDir::Name);
    for (const QFileInfo& jar : jars) {
        QString relative = QStringLiteral("mods/") + jar.fileName();
        if (relative.endsWith(QLatin1String(".disabled")))
            relative.chop(9);
        if (!packModFiles.contains(relative))
            orphans << jar.fileName();
    }
    if (!orphans.isEmpty()) {
        issues.append({ ProblemSeverity::Warning,
                        InstanceHealthCheckDialog::tr("Mods that are not part of %1 (%2)").arg(packLabel, orphans.size()),
                        InstanceHealthCheckDialog::tr("These jars in the mods folder are not in the pack's file list. They were either "
                                                      "added outside the pack, or left behind when the pack updated a mod to a different "
                                                      "file name:\n%1\n\n"
                                                      "If the game complains about duplicate or incompatible mods, remove or disable the "
                                                      "ones you did not add yourself.")
                            .arg(listNames(orphans)) });
    }
}
}  // namespace

InstanceHealthCheckDialog::InstanceHealthCheckDialog(MinecraftInstance* instance, QWidget* parent) : QDialog(parent), m_instance(instance)
{
    setWindowTitle(tr("Check instance"));
    setWindowModality(Qt::WindowModal);

    auto* layout = new QVBoxLayout(this);

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setWordWrap(true);
    layout->addWidget(m_summaryLabel);

    m_busyBar = new QProgressBar(this);
    m_busyBar->setRange(0, 0);  // "busy" animation
    m_busyBar->setTextVisible(false);
    m_busyBar->setMaximumHeight(8);
    layout->addWidget(m_busyBar);

    m_issuesList = new QListWidget(this);
    m_issuesList->setWordWrap(true);
    m_issuesList->setAlternatingRowColors(true);
    m_issuesList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    layout->addWidget(m_issuesList, 1);

    m_rerunButton = new QPushButton(tr("Check Again"), this);
    m_modsFolderButton = new QPushButton(tr("Open Mods Folder"), this);
    m_copyButton = new QPushButton(tr("Copy Report"), this);
    m_closeButton = new QPushButton(tr("Close"), this);
    m_closeButton->setDefault(true);
    connect(m_rerunButton, &QPushButton::clicked, this, &InstanceHealthCheckDialog::startChecks);
    connect(m_modsFolderButton, &QPushButton::clicked, this,
            [this] { DesktopServices::openPath(m_instance->modsRoot(), true); });
    connect(m_copyButton, &QPushButton::clicked, this, [this] { GuiUtil::setClipboardText(reportText()); });
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);

    auto* buttonRow = new QHBoxLayout();
    buttonRow->addWidget(m_rerunButton);
    buttonRow->addWidget(m_modsFolderButton);
    buttonRow->addWidget(m_copyButton);
    buttonRow->addStretch(1);
    buttonRow->addWidget(m_closeButton);
    layout->addLayout(buttonRow);

    m_jarWatcher = new QFutureWatcher<QStringList>(this);
    connect(m_jarWatcher, &QFutureWatcher<QStringList>::finished, this, &InstanceHealthCheckDialog::onJarScanFinished);

    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(150);
    connect(m_pollTimer, &QTimer::timeout, this, &InstanceHealthCheckDialog::pollModList);

    // a sensible default that never runs off the screen; restoreGeometry (below) may override it
    const QSize available = screen() ? screen()->availableGeometry().size() : QSize(1280, 720);
    const QSize maxSize(available.width() * 9 / 10, available.height() * 9 / 10);
    resize(QSize(600, 520).boundedTo(maxSize));

    // remember the window size between runs, like the settings window does
    GuiUtil::rememberGeometry(this, "InstanceCheckGeometry");

    startChecks();
}

void InstanceHealthCheckDialog::startChecks()
{
    if (m_jarWatcher->isRunning())
        return;  // already checking (the button is disabled, but a stray signal must not start a second scan)

    m_issues.clear();
    m_issuesList->clear();
    m_issuesList->hide();
    m_jarsDone = false;
    m_modListDone = false;
    m_modListUpdated = false;

    m_summaryLabel->setText(tr("Checking <b>%1</b>…").arg(m_instance->name().toHtmlEscaped()));
    m_busyBar->show();
    m_rerunButton->setEnabled(false);
    m_copyButton->setEnabled(false);

    runQuickChecks();

    // the slow part: jars are opened off the UI thread ...
    m_jarWatcher->setFuture(QtConcurrent::run(scanBrokenJars, FS::PathCombine(m_instance->gameRoot(), "mods")));

    // ... and the mod list is refreshed, then given a moment to parse the jars it found
    auto* mods = m_instance->loaderModList();
    if (!QDir(m_instance->modsRoot()).exists()) {
        m_modListDone = true;
    } else {
        connect(mods, &ResourceFolderModel::updateFinished, this, [this] { m_modListUpdated = true; }, Qt::SingleShotConnection);
        const bool started = mods->update();
        // not started = a scan is already running (its signal will come), or the list is already loaded
        if (!started && mods->rowCount() > 0)
            m_modListUpdated = true;
        m_waited.restart();
        m_pollTimer->start();
    }
    finishIfDone();
}

void InstanceHealthCheckDialog::runQuickChecks()
{
    checkJava(m_instance, m_issues);
    checkComponents(m_instance, m_issues);
    checkMemory(m_instance, m_issues);
    checkDiskSpace(m_instance, m_issues);
    checkManagedPackFiles(m_instance, m_issues);
}

void InstanceHealthCheckDialog::onJarScanFinished()
{
    const QStringList broken = m_jarWatcher->result();
    if (!broken.isEmpty()) {
        m_issues.append({ ProblemSeverity::Error, tr("Broken mod files (%1)").arg(broken.size()),
                          tr("These files in the mods folder are empty or damaged, and usually make the game "
                             "crash on startup:\n%1\n\nDelete or re-download them (a fresh download of the "
                             "same version is usually enough).")
                              .arg(listNames(broken)) });
    }
    m_jarsDone = true;
    finishIfDone();
}

void InstanceHealthCheckDialog::pollModList()
{
    auto* mods = m_instance->loaderModList();
    // The timeout covers a scan that never reports back; whatever the list holds by then is what we judge.
    const bool timedOut = m_waited.elapsed() > 10000;
    if (!timedOut && (!m_modListUpdated || mods->hasPendingParseTasks()))
        return;

    m_pollTimer->stop();
    runModListChecks();
    m_modListDone = true;
    finishIfDone();
}

void InstanceHealthCheckDialog::runModListChecks()
{
    const auto mods = m_instance->loaderModList()->allMods();

    // conflicts between mods: duplicates, declared incompatibilities, missing or disabled dependencies
    for (const auto& conflict : ModConflictChecker::check(mods)) {
        m_issues.append({ conflict.severity == ModConflictChecker::Severity::Error ? ProblemSeverity::Error : ProblemSeverity::Warning,
                          conflict.title, conflict.detail });
    }

    // mods that say they were not made for this game version (only known for mods installed through the launcher)
    QStringList wrongVersion;
    for (const Mod* mod : mods) {
        if (!mod->enabled())
            continue;
        // conflictIssues() are reported above, what is left is the game version check
        if (mod->issues().size() > mod->conflictIssues().size())
            wrongVersion << mod->fileinfo().fileName();
    }
    if (!wrongVersion.isEmpty()) {
        m_issues.append({ ProblemSeverity::Warning, tr("Mods that may not fit this game version (%1)").arg(wrongVersion.size()),
                          tr("These mods are not marked as compatible with this instance's Minecraft version:\n%1\n\n"
                             "They may still work, but if the game crashes, update or remove them first.")
                              .arg(listNames(wrongVersion)) });
    }
}

void InstanceHealthCheckDialog::finishIfDone()
{
    if (m_jarsDone && m_modListDone)
        showResults();
}

void InstanceHealthCheckDialog::showResults()
{
    m_busyBar->hide();
    m_rerunButton->setEnabled(true);
    m_copyButton->setEnabled(true);

    // worst first, otherwise in the order the checks ran
    std::stable_sort(m_issues.begin(), m_issues.end(), [](const Issue& a, const Issue& b) { return a.severity > b.severity; });

    int errors = 0;
    int warnings = 0;
    for (const auto& issue : std::as_const(m_issues)) {
        if (issue.severity == ProblemSeverity::Error)
            ++errors;
        else if (issue.severity == ProblemSeverity::Warning)
            ++warnings;
    }

    const QString name = m_instance->name().toHtmlEscaped();
    if (m_issues.isEmpty()) {
        m_summaryLabel->setText(tr("<b>%1</b> looks good - Java was found, the mods have no broken files or conflicts, and the loader "
                                   "version checks out. Nothing stood in the way of launching.")
                                    .arg(name));
        m_issuesList->hide();
        return;
    }

    if (errors > 0) {
        m_summaryLabel->setText(tr("<b>%1</b> has %n problem(s) that can stop the game from starting", "", errors).arg(name) +
                                (warnings > 0 ? tr(", and %n warning(s) worth a look", "", warnings) : QString()) +
                                tr(". The details are below each entry."));
    } else {
        m_summaryLabel->setText(tr("<b>%1</b> should start, but has %n warning(s) worth a look. The details are below each entry.", "",
                                   warnings)
                                    .arg(name));
    }

    for (const auto& issue : std::as_const(m_issues)) {
        // QListWidget items are plain text (no HTML), and constructing with a parent already adds the item
        const QString text = QStringLiteral("%1\n%2").arg(issue.title, issue.detail);
        auto* item = new QListWidgetItem(severityIcon(issue.severity), text, m_issuesList);
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
    }
    m_issuesList->show();
}

QString InstanceHealthCheckDialog::reportText() const
{
    QStringList lines;
    lines << tr("Instance check for \"%1\"").arg(m_instance->name());
    if (m_issues.isEmpty()) {
        lines << tr("No problems found.");
    }
    for (const auto& issue : m_issues) {
        lines << QString();
        lines << QStringLiteral("[%1] %2").arg(severityWord(issue.severity), issue.title);
        lines << issue.detail;
    }
    return lines.join(QStringLiteral("\n"));
}
