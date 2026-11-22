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
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *      Copyright 2026 PollyMC-Continued Contributors
 *
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *          http://www.apache.org/licenses/LICENSE-2.0
 *
 *      Unless required by applicable law or agreed to in writing, software
 *      distributed under the License is distributed on an "AS IS" BASIS,
 *      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *      See the License for the specific language governing permissions and
 *      limitations under the License.
 */

#include "BedrockLaunchSteps.h"

#include <utility>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>

#include "Application.h"
#include "BedrockArtifacts.h"
#include "BedrockInstallTask.h"
#include "BedrockVersionList.h"
#include "FileSystem.h"
#include "GdkPackageInstaller.h"
#include "GdkProtonEngineProvider.h"
#include "LicensingClient.h"
#include "MessageLevel.h"
#include "XodusLicensingClient.h"
#include "tasks/SequentialTask.h"

#if defined(Q_OS_WIN)
namespace {

/*
 * The GDK-era game data folder: %APPDATA%\Minecraft Bedrock (the Phase 0
 * design's verified location, the same folder the Linux prefix mirrors under
 * drive_c/users/steamuser/AppData/Roaming).
 */
QString gdkEraDataFolder()
{
    const QString appData = qEnvironmentVariable("APPDATA");
    if (appData.isEmpty()) {
        return QString();
    }
    return FS::PathCombine(appData, QStringLiteral("Minecraft Bedrock"));
}

/*
 * The legacy UWP era's game data folder: the registered package's LocalState
 * under %LOCALAPPDATA%\Packages. The family name comes from the installed
 * package's AppUserModelID ("<family>!<application id>").
 */
QString uwpEraDataFolder(const QString& installedFullName)
{
    const QString localAppData = qEnvironmentVariable("LOCALAPPDATA");
    if (localAppData.isEmpty() || installedFullName.isEmpty()) {
        return QString();
    }
    QString family = GdkPackageInstaller::aumidForPackage(installedFullName);
    family = family.contains(QLatin1Char('!')) ? family.section(QLatin1Char('!'), 0, 0) : QString();
    if (family.isEmpty()) {
        /* "Name_Version_Arch__PublisherId" -> "Name_PublisherId". */
        family = installedFullName.section(QLatin1Char('_'), 0, 0) + QStringLiteral("_")
                 + installedFullName.section(QLatin1Char('_'), -1);
    }
    if (family.isEmpty() || family.endsWith(QLatin1Char('_'))) {
        return QString();
    }
    return FS::PathCombine(FS::PathCombine(localAppData, QStringLiteral("Packages"), family), QStringLiteral("LocalState"));
}

/*
 * Creates the NTFS junction at linkPath that points at targetPath (junctions
 * need no administrator rights). Returns false when the junction is not in
 * place afterwards.
 */
bool createJunction(const QString& linkPath, const QString& targetPath)
{
    QProcess mklink;
    mklink.start(QStringLiteral("cmd"),
                 { QStringLiteral("/c"),
                   QStringLiteral("mklink"),
                   QStringLiteral("/J"),
                   QDir::toNativeSeparators(linkPath),
                   QDir::toNativeSeparators(targetPath) });
    if (!mklink.waitForStarted(5000) || !mklink.waitForFinished(15000)) {
        return false;
    }
    return mklink.exitCode() == 0 && QFileInfo(linkPath).isSymLink();
}

}  // namespace
#endif  // Q_OS_WIN

////// BedrockEnsureGameStep //////

BedrockEnsureGameStep::BedrockEnsureGameStep(LaunchTask* parent, BedrockInstance* instance) : LaunchStep(parent), m_bedrockInstance(instance)
{}

void BedrockEnsureGameStep::executeTask()
{
    const QString versionId = m_bedrockInstance->bedrockVersion();
    if (versionId.isEmpty()) {
        emitFailed(tr("This Bedrock instance has no game version selected. Pick one on the instance's Version page."));
        return;
    }
    const BedrockVersion::Ptr version = BedrockVersionList::get()->findBedrockVersion(versionId);
    if (!version) {
        emitFailed(tr("Bedrock version %1 is not known to the version list. Open the instance's Version page once while online and "
                      "try again.")
                       .arg(versionId));
        return;
    }

    if (BedrockInstallTask::isGameInstalled(m_bedrockInstance->instanceRoot(), version)) {
        emit logLine(tr("The Bedrock %1 game is already installed.").arg(versionId), MessageLevel::Launcher);
        /* The Windows extras (legacy registration, helper) may still be pending. */
#if defined(Q_OS_WIN)
        registerLegacyUwpPackage(version);
        deployWindowsLaunchHelper(version);
#endif
        emitSucceeded();
        return;
    }

    runInstall();
}

bool BedrockEnsureGameStep::abort()
{
    m_aborted = true;
    if (m_installTask && m_installTask->isRunning()) {
        m_installTask->abort();
    }
    emitAborted();
    return true;
}

void BedrockEnsureGameStep::runInstall()
{
    emit logLine(tr("Installing the Bedrock %1 game files...").arg(m_bedrockInstance->bedrockVersion()), MessageLevel::Launcher);
    m_installTask = BedrockInstallTask::create(m_bedrockInstance);
    connect(m_installTask.get(), &Task::succeeded, this, [this]() { installSucceeded(); });
    connect(m_installTask.get(), &Task::failed, this, [this](const QString& reason) { installFailed(reason); });
    connect(m_installTask.get(), &Task::status, this, [this](const QString& status) { emit logLine(status, MessageLevel::Launcher); });
    m_installTask->start();
}

void BedrockEnsureGameStep::installSucceeded()
{
#if defined(Q_OS_WIN)
    const BedrockVersion::Ptr version = BedrockVersionList::get()->findBedrockVersion(m_bedrockInstance->bedrockVersion());
    if (version) {
        registerLegacyUwpPackage(version);
        deployWindowsLaunchHelper(version);
    }
#endif
    emitSucceeded();
}

void BedrockEnsureGameStep::installFailed(const QString& reason)
{
    emitFailed(tr("The Bedrock game could not be installed: %1").arg(reason));
}

void BedrockEnsureGameStep::registerLegacyUwpPackage(const BedrockVersion::Ptr& version)
{
    if (version->packaging() != BedrockVersion::Packaging::Appx) {
        return;
    }
    /*
     * The loose folder registration of the legacy pipeline happens here, with
     * the instance folder committed at its final location (the registered
     * package points into it).
     */
    const QString manifestPath = FS::PathCombine(FS::PathCombine(m_bedrockInstance->instanceRoot(), QStringLiteral("game")),
                                                QStringLiteral("AppxManifest.xml"));
    const QString installed = GdkPackageInstaller::installedMinecraftPackageFullName();
    const QString expected = BedrockInstallTask::expectedWindowsPackageVersion(version);
    if (!installed.isEmpty() && !expected.isEmpty() && installed.contains(expected)) {
        emit logLine(tr("The legacy UWP package of Bedrock %1 is already registered.").arg(version->versionId()), MessageLevel::Launcher);
        return;
    }
    if (!QFileInfo::exists(manifestPath)) {
        emit logLine(tr("The extracted legacy UWP package is incomplete (no AppxManifest.xml).").arg(version->versionId()),
                     MessageLevel::Warning);
        return;
    }
    QString error;
    if (!GdkPackageInstaller::registerExtractedPackage(manifestPath, &error)) {
        emit logLine(tr("Registering the legacy UWP package failed: %1").arg(error), MessageLevel::Warning);
        emit logLine(tr("Windows developer mode must be enabled for legacy UWP versions."), MessageLevel::Warning);
    } else {
        emit logLine(tr("Registered the legacy UWP package of Bedrock %1 in developer mode.").arg(version->versionId()),
                     MessageLevel::Launcher);
    }
}

void BedrockEnsureGameStep::deployWindowsLaunchHelper(const BedrockVersion::Ptr& version)
{
    Q_UNUSED(version)
    /*
     * Optional exact-version pinning: BedrockLauncher's gamelaunchhelper.dll
     * replaces the package's own helper (which forwards to the bootstrapper
     * that auto-updates the game). Only deployed when the user points the
     * instance at a helper DLL; failures are logged, never fatal.
     */
    const QString helperSource = m_bedrockInstance->settings()->get("BedrockGdkLaunchHelperPath").toString();
    if (helperSource.isEmpty() || !QFileInfo::exists(helperSource)) {
        return;
    }
    const QString installed = GdkPackageInstaller::installedMinecraftPackageFullName();
    if (installed.isEmpty()) {
        return;
    }
    const QString packageDir = GdkPackageInstaller::installedPackageLocation(installed);
    if (packageDir.isEmpty()) {
        return;
    }
    const QString destination = FS::PathCombine(packageDir, QStringLiteral("gamelaunchhelper.dll"));
    QFile::remove(destination);
    if (QFile::copy(helperSource, destination)) {
        emit logLine(tr("Deployed the launch helper DLL that pins the exact installed version."), MessageLevel::Launcher);
    } else {
        emit logLine(tr("Could not deploy the launch helper DLL (the game may auto-update to the latest version instead)."),
                     MessageLevel::Warning);
    }
}

////// BedrockWindowsLaunchStep //////

BedrockWindowsLaunchStep::BedrockWindowsLaunchStep(LaunchTask* parent, BedrockInstance* instance)
    : LaunchStep(parent), m_bedrockInstance(instance)
{}

void BedrockWindowsLaunchStep::executeTask()
{
#if defined(Q_OS_WIN)
    const BedrockVersion::Ptr version = BedrockVersionList::get()->findBedrockVersion(m_bedrockInstance->bedrockVersion());
    if (!version) {
        emitFailed(tr("Bedrock version %1 is not known to the version list.").arg(m_bedrockInstance->bedrockVersion()));
        return;
    }

    const QString installed = GdkPackageInstaller::installedMinecraftPackageFullName();
    if (installed.isEmpty()) {
        emitFailed(tr("No Bedrock game package is installed for this user. Launch failed before the game install could complete."));
        return;
    }

    /* The data profile first: the launched instance's data folder becomes the game's active one. */
    redirectGameData(version);

    if (version->packaging() == BedrockVersion::Packaging::Appx) {
        /* The legacy UWP pipeline: activation through the game's protocol. */
        setStatus(tr("Launching Bedrock %1 through its protocol").arg(version->versionId()));
        QString error;
        if (!GdkPackageInstaller::activateProtocol(QStringLiteral("minecraft:"), &error)) {
            emitFailed(tr("Could not activate the registered Bedrock game: %1").arg(error));
            return;
        }
        emit logLine(tr("Requested the Bedrock %1 launch through the minecraft: protocol.").arg(version->versionId()),
                     MessageLevel::Launcher);
        emitSucceeded();
        return;
    }

    /* The GDK pipeline: activation through the installed package's AppUserModelID. */
    setStatus(tr("Launching the installed Bedrock game"));
    const QString aumid = GdkPackageInstaller::aumidForPackage(installed);
    if (aumid.isEmpty()) {
        emitFailed(tr("Could not determine the AppUserModelID of the installed Bedrock package (%1).").arg(installed));
        return;
    }
    QString error;
    const qint64 pid = GdkPackageInstaller::activateAumid(aumid, QString(), &error);
    if (pid < 0) {
        emitFailed(tr("Could not activate the installed Bedrock game: %1").arg(error));
        return;
    }
    emit logLine(tr("Activated the installed Bedrock game (PID %1).").arg(pid), MessageLevel::Launcher);
    emitSucceeded();
#else
    emitFailed(tr("The Windows Bedrock backend is only available on Windows."));
#endif
}

void BedrockWindowsLaunchStep::redirectGameData(const BedrockVersion::Ptr& version)
{
#if defined(Q_OS_WIN)
    if (!m_bedrockInstance->settings()->get("BedrockSeparateGameData").toBool()) {
        emit logLine(tr("Game data: shared (this instance keeps the game's default data folder)."), MessageLevel::Launcher);
        return;
    }

    const QString installed = GdkPackageInstaller::installedMinecraftPackageFullName();
    const QString dataFolder = version->isLegacyUwp() ? uwpEraDataFolder(installed) : gdkEraDataFolder();
    if (dataFolder.isEmpty()) {
        emit logLine(tr("Game data: the game's data folder could not be determined, so it was not redirected."),
                     MessageLevel::Warning);
        return;
    }
    const QString instanceData = BedrockPaths::instanceGameDataDir(m_bedrockInstance);
    if (instanceData.isEmpty() || !FS::ensureFolderPathExists(instanceData)) {
        emit logLine(tr("Game data: this instance's data folder could not be created, so it was not redirected."),
                     MessageLevel::Warning);
        return;
    }

    const QFileInfo info(dataFolder);
    if (info.isSymLink()) {
        /* A junction in place (another instance's data): remove the link only and repoint it. */
        if (!QDir().rmdir(dataFolder)) {
            emit logLine(tr("Game data: the existing data link (%1) could not be replaced; keeping it as-is.")
                             .arg(QDir::toNativeSeparators(dataFolder)),
                         MessageLevel::Warning);
            return;
        }
    } else if (info.isDir()) {
        if (!QDir(instanceData).exists() || QDir(instanceData).isEmpty()) {
            /*
             * First redirection while the game already has data: adopt it into
             * this instance (one move on the same drive, the usual case), after
             * an explicit confirmation.
             */
            const auto answer = QMessageBox::question(nullptr,
                                                      tr("Move the Bedrock game data into this instance?"),
                                                      tr("The game currently keeps its data (worlds, resource packs, "
                                                         "settings) in:\n%1\n\nMove it into this instance and link the game "
                                                         "to the instance's data folder from now on?\n\nEach Bedrock "
                                                         "instance then keeps its own data; the instance launched last is "
                                                         "the active one while several run.")
                                                          .arg(QDir::toNativeSeparators(dataFolder)),
                                                      QMessageBox::Yes | QMessageBox::No,
                                                      QMessageBox::No);
            if (answer != QMessageBox::Yes) {
                emit logLine(tr("Game data: kept in the game's default folder (the instance shares it)."), MessageLevel::Launcher);
                return;
            }
            /*
             * The instance's data folder was created empty above; the rename
             * needs it out of the way (QDir::rename does not replace).
             */
            QDir().rmdir(instanceData);
            if (!QDir().rename(dataFolder, instanceData)) {
                emit logLine(tr("Game data: moving the existing data into the instance failed (different drives, perhaps); "
                               "it stays shared."),
                             MessageLevel::Warning);
                return;
            }
        } else {
            /*
             * A real folder in the way while this instance already has its own
             * data (the game recreated its folder on a reinstall): back it up
             * next to the link location and keep the instance data.
             */
            const QString backup = dataFolder + QStringLiteral(".pre-instance");
            QDir(backup).removeRecursively();
            if (!QDir().rename(dataFolder, backup)) {
                emit logLine(tr("Game data: the game's newly created data folder (%1) could not be moved aside; it stays shared.")
                                 .arg(QDir::toNativeSeparators(dataFolder)),
                             MessageLevel::Warning);
                return;
            }
            emit logLine(tr("Game data: the game's newly created folder was kept as \"%1\".")
                             .arg(QDir::toNativeSeparators(backup)),
                         MessageLevel::Launcher);
        }
    }

    if (!createJunction(dataFolder, instanceData)) {
        emit logLine(tr("Game data: linking %1 to this instance failed; the game keeps its own folder.")
                         .arg(QDir::toNativeSeparators(dataFolder)),
                     MessageLevel::Warning);
        return;
    }
    emit logLine(tr("Game data: %1 -> this instance's data folder.").arg(QDir::toNativeSeparators(dataFolder)),
                 MessageLevel::Launcher);
#else
    Q_UNUSED(version)
#endif
}

////// BedrockLinuxLaunchStep //////

BedrockLinuxLaunchStep::BedrockLinuxLaunchStep(LaunchTask* parent, BedrockInstance* instance)
    : LaunchStep(parent), m_bedrockInstance(instance)
{}

void BedrockLinuxLaunchStep::executeTask()
{
#if defined(Q_OS_LINUX)
    runSetup();
#else
    emitFailed(tr("The Linux Bedrock backend (experimental) is only available on Linux."));
#endif
}

bool BedrockLinuxLaunchStep::abort()
{
    m_aborted = true;
    if (m_setupTask && m_setupTask->isRunning()) {
        m_setupTask->abort();
    }
    if (m_gameProcess.state() != LoggedProcess::NotRunning) {
        m_gameProcess.kill();
    }
    emitAborted();
    return true;
}

void BedrockLinuxLaunchStep::runSetup()
{
    /*
     * First launch downloads everything the backend needs (the engine, umu,
     * the GDK dependency DLLs and the licensing client - all hash-pinned);
     * after that this is a quick in-place check.
     */
    const QString xodusOverride = m_bedrockInstance->settings()->get("BedrockXodusPath").toString();
    QString xodusReason;
    const bool licensingReady = !XodusLicensingClient::get()->resolveBinary(xodusOverride, &xodusReason).isEmpty();
    const bool engineReady = GdkProtonEngineProvider::get()->isInstalled();

    if (licensingReady && engineReady) {
        setupFinished();
        return;
    }

    emit logLine(tr("Preparing the Bedrock Linux backend (this downloads the engine and the licensing client on first use)..."),
                 MessageLevel::Launcher);
    auto setup = makeShared<SequentialTask>(tr("Bedrock Linux backend setup"));
    if (!engineReady) {
        setup->addTask(GdkProtonEngineProvider::get()->ensureInstalled());
    }
    if (!licensingReady) {
        setup->addTask(XodusLicensingClient::get()->ensureManagedCopy());
    }
    m_setupTask = setup;
    connect(m_setupTask.get(), &Task::succeeded, this, &BedrockLinuxLaunchStep::setupFinished);
    connect(m_setupTask.get(), &Task::failed, this, &BedrockLinuxLaunchStep::setupFailed);
    connect(m_setupTask.get(), &Task::status, this, [this](const QString& status) { emit logLine(status, MessageLevel::Launcher); });
    m_setupTask->start();
}

void BedrockLinuxLaunchStep::setupFinished()
{
    const QString xodusOverride = m_bedrockInstance->settings()->get("BedrockXodusPath").toString();
    QString xodusReason;
    const QString xodusBinary = XodusLicensingClient::get()->resolveBinary(xodusOverride, &xodusReason);
    if (xodusBinary.isEmpty()) {
        emitFailed(tr("The licensing client is not available: %1").arg(xodusReason));
        return;
    }
    const QString engineRoot = GdkProtonEngineProvider::get()->engineRoot();
    if (engineRoot.isEmpty()) {
        emitFailed(tr("The GDK-Proton engine is not installed."));
        return;
    }

    emit logLine(tr("Engine: %1").arg(engineRoot), MessageLevel::Launcher);
    emit logLine(tr("Licensing client: %1").arg(xodusBinary), MessageLevel::Launcher);
    emit logLine(tr("Backend: GDK-Proton (experimental). Achievements do not unlock; the game needs internet at every start."),
                 MessageLevel::Launcher);

    if (!checkVulkan()) {
        return;
    }
    writeRegistryToken();
    startGameProcess();
}

void BedrockLinuxLaunchStep::setupFailed(const QString& reason)
{
    emitFailed(tr("The Bedrock Linux backend could not be set up: %1").arg(reason));
}

bool BedrockLinuxLaunchStep::checkVulkan()
{
    /*
     * The D3D12-only renderer means a Vulkan 1.3-class driver is a hard
     * requirement (RADV or a current NVIDIA driver; llvmpipe and virtual GPUs
     * do not work). A missing vulkaninfo is a warning, not a stop: the driver
     * may still work.
     */
    QProcess vulkaninfo;
    vulkaninfo.start(QStringLiteral("vulkaninfo"), { QStringLiteral("--summary") });
    if (!vulkaninfo.waitForStarted(5000) || !vulkaninfo.waitForFinished(15000)) {
        emit logLine(tr("vulkaninfo is not available on this system; skipping the Vulkan driver check (the game needs a "
                        "Vulkan 1.3-class driver)."),
                     MessageLevel::Warning);
        return true;
    }
    const QString output = QString::fromUtf8(vulkaninfo.readAllStandardOutput());
    const QRegularExpression pattern(QStringLiteral("apiVersion[\\s:]*([0-9]+)\\.([0-9]+)"));
    bool foundApiVersion = false;
    QRegularExpressionMatchIterator matches = pattern.globalMatch(output);
    while (matches.hasNext()) {
        const QRegularExpressionMatch match = matches.next();
        foundApiVersion = true;
        const int major = match.captured(1).toInt();
        const int minor = match.captured(2).toInt();
        if (major > 1 || (major == 1 && minor >= 3)) {
            emit logLine(tr("Vulkan %1.%2 found: the D3D12 renderer can run.").arg(major).arg(minor), MessageLevel::Launcher);
            return true;
        }
    }
    if (!foundApiVersion) {
        emit logLine(tr("vulkaninfo did not report a usable Vulkan API version; the game may fail to start."), MessageLevel::Warning);
        return true;
    }
    emitFailed(tr("No usable Vulkan 1.3 adapter was found. Minecraft Bedrock renders through D3D12 (vkd3d-proton), which needs a "
                  "Vulkan 1.3-class driver - RADV 26.1+ or an NVIDIA driver of the 595 series or newer is recommended; llvmpipe "
                  "and virtual GPUs do not work. A legacy OpenGL compatibility renderer is not part of this build."));
    return false;
}

void BedrockLinuxLaunchStep::writeRegistryToken()
{
    /*
     * Advanced: the in-game Xbox sign-in under WineGDK reads a Microsoft
     * account refresh token from the prefix registry. When the instance has
     * one configured, best-effort write it into the per-instance prefix; a
     * failure is logged, never fatal (see TESTING.md for verification).
     */
    const QString token = m_bedrockInstance->settings()->get("BedrockMsaRefreshToken").toString();
    if (token.isEmpty()) {
        return;
    }
    const QString umuRun = GdkProtonEngineProvider::get()->umuRunPath();
    const QString prefix = BedrockPaths::instancePrefixDir(m_bedrockInstance);
    if (umuRun.isEmpty() || prefix.isEmpty()) {
        return;
    }
    QProcess reg;
    reg.setProgram(umuRun);
    reg.setArguments({ QStringLiteral("reg"),
                       QStringLiteral("add"),
                       QStringLiteral("HKCU\\") + BedrockArtifacts::winegdkTokenRegistryKey(),
                       QStringLiteral("/v"),
                       BedrockArtifacts::winegdkTokenRegistryValue(),
                       QStringLiteral("/t"),
                       QStringLiteral("REG_SZ"),
                       QStringLiteral("/d"),
                       token,
                       QStringLiteral("/f") });
    reg.setProcessEnvironment(
        GdkProtonEngineProvider::launchEnvironment(GdkProtonEngineProvider::get()->engineRoot(), prefix, BedrockPaths::xodusHomeDir(),
                                                    BedrockPaths::instanceGameDir(m_bedrockInstance)));
    reg.start();
    if (reg.waitForStarted(5000) && reg.waitForFinished(60000) && reg.exitCode() == 0) {
        emit logLine(tr("Wrote the configured Microsoft account token into the instance prefix for the in-game sign-in."),
                     MessageLevel::Launcher);
    } else {
        emit logLine(tr("Could not write the Microsoft account token into the instance prefix; the in-game sign-in may fail."),
                     MessageLevel::Warning);
    }
}

void BedrockLinuxLaunchStep::startGameProcess()
{
    const QString xodusOverride = m_bedrockInstance->settings()->get("BedrockXodusPath").toString();
    QString xodusReason;
    const QString xodusBinary = XodusLicensingClient::get()->resolveBinary(xodusOverride, &xodusReason);
    if (xodusBinary.isEmpty()) {
        emitFailed(tr("The licensing client is not available: %1").arg(xodusReason));
        return;
    }
    const QString engineRoot = GdkProtonEngineProvider::get()->engineRoot();
    const QString prefix = BedrockPaths::instancePrefixDir(m_bedrockInstance);
    const QString gameDir = BedrockPaths::instanceGameDir(m_bedrockInstance);
    if (engineRoot.isEmpty() || prefix.isEmpty() || gameDir.isEmpty()) {
        emitFailed(tr("The Bedrock Linux backend is not set up (engine, prefix or game directory missing)."));
        return;
    }
    if (!FS::ensureFolderPathExists(prefix)) {
        emitFailed(tr("Could not create the per-instance Wine prefix."));
        return;
    }

    const QProcessEnvironment environment =
        GdkProtonEngineProvider::launchEnvironment(engineRoot, prefix, BedrockPaths::xodusHomeDir(), gameDir);

    /*
     * Default invocation: the licensing client starts the encrypted game
     * (the env carries WINEPREFIX/PROTONPATH for it). The command is
     * overridable per instance for advanced setups (TESTING.md documents the
     * placeholders).
     */
    QString command = m_bedrockInstance->settings()->get("BedrockLinuxLaunchCommand").toString();
    if (command.isEmpty()) {
        command = QStringLiteral("{xodus} run {game_dir}");
    }
    command.replace(QStringLiteral("{umu_run}"), GdkProtonEngineProvider::get()->umuRunPath());
    command.replace(QStringLiteral("{engine}"), engineRoot);
    command.replace(QStringLiteral("{prefix}"), prefix);
    command.replace(QStringLiteral("{xodus}"), xodusBinary);
    command.replace(QStringLiteral("{game_dir}"), gameDir);

    const QStringList pieces = command.split(' ', Qt::SkipEmptyParts);
    if (pieces.isEmpty()) {
        emitFailed(tr("The configured Bedrock launch command is empty."));
        return;
    }

    emit logLine(tr("Launch command: %1").arg(pieces.join(QStringLiteral(" "))), MessageLevel::Launcher);

    m_gameProcess.setProcessEnvironment(environment);
    m_gameProcess.setProgram(pieces.first());
    m_gameProcess.setArguments(pieces.mid(1));
    connect(&m_gameProcess, &LoggedProcess::log, this, &BedrockLinuxLaunchStep::gameLogLines);
    connect(&m_gameProcess, &LoggedProcess::stateChanged, this, &BedrockLinuxLaunchStep::gameProcessStateChanged);
    m_gameProcess.start();

    setStatus(tr("Bedrock is running (experimental)"));
}

void BedrockLinuxLaunchStep::gameLogLines(const QStringList& lines, MessageLevel level)
{
    if (!lines.isEmpty()) {
        emit logLines(lines, level);
    }
}

void BedrockLinuxLaunchStep::gameProcessStateChanged(LoggedProcess::State state)
{
    switch (state) {
        case LoggedProcess::Starting:
        case LoggedProcess::Running:
            break;
        case LoggedProcess::FailedToStart:
            emitFailed(tr("The Bedrock game process could not be started. Check the launch command above and the licensing "
                          "client's login state (run \"%1 login\" once).")
                           .arg(m_gameProcess.program()));
            break;
        case LoggedProcess::Finished:
            emit logLine(tr("The game process finished (exit code %1).").arg(m_gameProcess.exitCode()), MessageLevel::Launcher);
            emitSucceeded();
            break;
        case LoggedProcess::Crashed:
            emitFailed(tr("The game process crashed (exit code %1).").arg(m_gameProcess.exitCode()));
            break;
        case LoggedProcess::Aborted:
            break;
        default:
            break;
    }
}
