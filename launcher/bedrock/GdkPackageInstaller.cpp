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

#include "GdkPackageInstaller.h"

#if defined(Q_OS_WIN)

#include <thread>

#include <windows.h>
#include <shellapi.h>
#include <shobjidl.h>
#if defined(_MSC_VER)
/* C++/WinRT ships with the Windows SDK only; MinGW builds use PowerShell instead. */
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Management.Deployment.h>
#endif

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "FileSystem.h"

#if defined(_MSC_VER)
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "runtimeobject.lib")
#endif

namespace {

struct DeploymentOutcome {
    bool succeeded = false;
    long errorCode = 0;
    QString errorText;
};

#if defined(_MSC_VER)
/* One AddPackageAsync call, run on a worker thread (the GUI thread is an STA). */
DeploymentOutcome runAddPackageAsync(const QString& packagePath, bool developmentMode)
{
    DeploymentOutcome outcome{};
    const std::wstring widePackagePath = packagePath.toStdWString();
    std::thread worker([&outcome, widePackagePath, developmentMode]() {
        try {
            winrt::init_apartment(winrt::apartment_type::multi_threaded);
            winrt::Windows::Management::Deployment::PackageManager manager;
            const winrt::Windows::Foundation::Uri packageUri(widePackagePath);
            const winrt::Windows::Management::Deployment::DeploymentOptions options = developmentMode
                ? winrt::Windows::Management::Deployment::DeploymentOptions::DevelopmentMode
                : winrt::Windows::Management::Deployment::DeploymentOptions::None;
            auto operation = manager.AddPackageAsync(packageUri, nullptr, options);
            const winrt::Windows::Management::Deployment::DeploymentResult result = operation.get();
            outcome.errorCode = static_cast<long>(result.ExtendedErrorCode().value);
            outcome.errorText = QString::fromWCharArray(result.ErrorText().c_str());
            outcome.succeeded = outcome.errorCode == 0;
        } catch (const winrt::hresult_error& exception) {
            outcome.succeeded = false;
            outcome.errorCode = static_cast<long>(exception.code());
            outcome.errorText = QString::fromWCharArray(exception.message().c_str());
        }
    });
    worker.join();
    return outcome;
}

#else  // !_MSC_VER

/*
 * Runs a PowerShell script without a console window and returns its exit code
 * (-1 when PowerShell could not be started or timed out). Stdout and stderr are
 * returned through the out-parameters.
 */
int runPowerShell(const QString& script, QString* standardOutput, QString* standardError, int timeoutMs = 20 * 60 * 1000)
{
    if (GdkPackageInstaller::isRunningUnderWine()) {
        if (standardError) {
            *standardError = QObject::tr("Windows package deployment is not available under Wine.");
        }
        return -1;
    }
    QProcess process;
    process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* arguments) { arguments->flags |= CREATE_NO_WINDOW; });
    const QString wrapped = QStringLiteral(
                                "$ProgressPreference='SilentlyContinue'; $ErrorActionPreference='Stop'; "
                                "try { %1 } catch { [Console]::Error.WriteLine($_.Exception.Message); exit 1 }")
                                .arg(script);
    process.start(QStringLiteral("powershell.exe"),
                  { QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"), QStringLiteral("-ExecutionPolicy"),
                    QStringLiteral("Bypass"), QStringLiteral("-Command"), wrapped });
    if (!process.waitForStarted(10000)) {
        if (standardError) {
            *standardError = QObject::tr("PowerShell could not be started.");
        }
        return -1;
    }
    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        process.waitForFinished(2000);
        if (standardError) {
            *standardError = QObject::tr("PowerShell timed out.");
        }
        return -1;
    }
    if (standardOutput) {
        *standardOutput = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    }
    if (standardError) {
        *standardError = QString::fromUtf8(process.readAllStandardError()).trimmed();
    }
    return process.exitCode();
}

/* Single-quotes a string for PowerShell. */
QString psQuote(const QString& text)
{
    QString escaped = text;
    escaped.replace(QLatin1Char('\''), QStringLiteral("''"));
    return QStringLiteral("'") + escaped + QStringLiteral("'");
}

/* One Add-AppxPackage call; -Register is the developer-mode registration of an extracted manifest. */
DeploymentOutcome runAddPackageAsync(const QString& packagePath, bool developmentMode)
{
    DeploymentOutcome outcome{};
    const QString script = developmentMode ? QStringLiteral("Add-AppxPackage -Register %1").arg(psQuote(QDir::toNativeSeparators(packagePath)))
                                           : QStringLiteral("Add-AppxPackage -Path %1").arg(psQuote(QDir::toNativeSeparators(packagePath)));
    QString standardOutput;
    QString standardError;
    const int exitCode = runPowerShell(script, &standardOutput, &standardError);
    outcome.succeeded = exitCode == 0;
    if (!outcome.succeeded) {
        outcome.errorText = standardError.isEmpty() ? standardOutput : standardError;
        /* The deployment service's HRESULT is part of the message text; surface it as the code. */
        const QRegularExpressionMatch match = QRegularExpression(QStringLiteral("0x([0-9A-Fa-f]{8})")).match(outcome.errorText);
        outcome.errorCode = match.hasMatch() ? static_cast<long>(match.captured(1).toUInt(nullptr, 16)) : static_cast<long>(exitCode == 0 ? 1 : exitCode);
    }
    return outcome;
}

#endif  // _MSC_VER

/* True when the error text points at package dependencies as the blocker. */
bool looksLikeDependencyFailure(const QString& errorText)
{
    const QString lowered = errorText.toLower();
    return lowered.contains(QStringLiteral("dependency")) || lowered.contains(QStringLiteral("dependencies"));
}

/*
 * Rewrites an AppxManifest.xml without its <PackageDependency> entries
 * (backing the original up next to it). The best-effort legacy-UWP fallback
 * for machines without the framework packages the manifest asks for.
 */
bool stripManifestDependencies(const QString& manifestPath)
{
    QFile manifestFile(manifestPath);
    if (!manifestFile.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QByteArray original = manifestFile.readAll();
    manifestFile.close();

    QByteArray rewritten;
    {
        QXmlStreamReader reader(original);
        QXmlStreamWriter writer(&rewritten);
        writer.setAutoFormatting(false);
        int dependenciesDepth = 0;
        while (!reader.atEnd()) {
            reader.readNext();
            if (reader.isStartElement()) {
                const QString name = reader.name().toString();
                if (name.compare(QLatin1String("Dependencies"), Qt::CaseInsensitive) == 0) {
                    dependenciesDepth++;
                    /* Enter but do not write: its children are rewritten selectively. */
                    continue;
                }
                if (dependenciesDepth > 0 && name.compare(QLatin1String("PackageDependency"), Qt::CaseInsensitive) == 0) {
                    /* Dropped entirely. */
                    continue;
                }
                writer.writeStartElement(name);
                for (const QXmlStreamAttribute& attribute : reader.attributes()) {
                    writer.writeAttribute(attribute.name().toString(), attribute.value().toString());
                }
            } else if (reader.isCDATA()) {
                writer.writeCDATA(reader.text().toString());
            } else if (reader.isCharacters()) {
                if (dependenciesDepth == 0 && !reader.isWhitespace()) {
                    writer.writeCharacters(reader.text().toString());
                }
            } else if (reader.isComment()) {
                writer.writeComment(reader.text().toString());
            } else if (reader.isEndElement()) {
                const QString name = reader.name().toString();
                if (dependenciesDepth > 0 && name.compare(QLatin1String("Dependencies"), Qt::CaseInsensitive) == 0) {
                    dependenciesDepth--;
                    continue;
                }
                if (dependenciesDepth > 0 && name.compare(QLatin1String("PackageDependency"), Qt::CaseInsensitive) == 0) {
                    continue;
                }
                writer.writeEndElement();
            } else if (reader.isStartDocument()) {
                writer.writeStartDocument();
            } else if (reader.isEndDocument()) {
                writer.writeEndDocument();
            }
        }
        if (reader.hasError()) {
            return false;
        }
    }

    const QString backupPath = QFileInfo(manifestPath).absolutePath() + QStringLiteral("/AppxManifest-original.xml");
    QFile::remove(backupPath);
    QFile::rename(manifestPath, backupPath);

    QSaveFile patched(manifestPath);
    if (!patched.open(QIODevice::WriteOnly)) {
        return false;
    }
    patched.write(rewritten);
    return patched.commit();
}

/* The first application id of a package manifest, for the AppUserModelID. */
QString manifestApplicationId(const QString& manifestPath)
{
    QFile manifestFile(manifestPath);
    if (!manifestFile.open(QIODevice::ReadOnly)) {
        return QString();
    }
    QXmlStreamReader reader(&manifestFile);
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement() && reader.name().compare(QLatin1String("Application"), Qt::CaseInsensitive) == 0) {
            const QString applicationId = reader.attributes().value(QStringLiteral("Id")).toString();
            manifestFile.close();
            return applicationId;
        }
    }
    manifestFile.close();
    return QString();
}

/* True when the package full name belongs to a Minecraft for Windows package. */
bool isMinecraftPackage(const QString& fullName)
{
    return fullName.startsWith(QStringLiteral("Microsoft.MinecraftUWP_"))
        || fullName.startsWith(QStringLiteral("Microsoft.MinecraftWindowsBeta_"));
}

}  // namespace

namespace GdkPackageInstaller {

bool isSupported()
{
    return true;
}

bool isRunningUnderWine()
{
    static const bool underWine = []() {
        const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
        return ntdll && GetProcAddress(ntdll, "wine_get_version") != nullptr;
    }();
    return underWine;
}

bool installPackageFile(const QString& packageFilePath, QString* error)
{
    if (!QFileInfo::exists(packageFilePath)) {
        if (error) {
            *error = QObject::tr("The package file does not exist: %1").arg(packageFilePath);
        }
        return false;
    }
    const DeploymentOutcome outcome = runAddPackageAsync(packageFilePath, false);
    if (!outcome.succeeded) {
        if (error) {
            *error = QObject::tr("Windows did not install the package (code 0x%1): %2")
                         .arg(QString::number(outcome.errorCode, 16), outcome.errorText);
        }
        return false;
    }
    return true;
}

bool registerExtractedPackage(const QString& manifestPath, QString* error)
{
    if (!QFileInfo::exists(manifestPath)) {
        if (error) {
            *error = QObject::tr("The extracted package manifest does not exist: %1").arg(manifestPath);
        }
        return false;
    }
    DeploymentOutcome outcome = runAddPackageAsync(manifestPath, true);
    if (outcome.succeeded) {
        return true;
    }
    /*
     * Retry once with a manifest whose package dependencies were stripped: the
     * legacy-UWP pipeline of machines without the asked-for framework
     * packages. Windows' developer mode is a prerequisite either way.
     */
    if (looksLikeDependencyFailure(outcome.errorText) && stripManifestDependencies(manifestPath)) {
        outcome = runAddPackageAsync(manifestPath, true);
        if (outcome.succeeded) {
            return true;
        }
    }
    if (error) {
        *error = QObject::tr("Windows did not register the package (code 0x%1): %2. Developer mode must be enabled for "
                             "legacy UWP packages.")
                     .arg(QString::number(outcome.errorCode, 16), outcome.errorText);
    }
    return false;
}

QString installedMinecraftPackageFullName()
{
#if defined(_MSC_VER)
    try {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        winrt::Windows::Management::Deployment::PackageManager manager;
        for (auto const& package : manager.FindPackagesForUser(winrt::hstring())) {
            const QString fullName = QString::fromWCharArray(package.Id().FullName().c_str());
            if (isMinecraftPackage(fullName)) {
                return fullName;
            }
        }
    } catch (const winrt::hresult_error& exception) {
        qWarning() << "Could not enumerate the installed packages:" << QString::fromWCharArray(exception.message().c_str());
    }
    return QString();
#else
    QString standardOutput;
    QString standardError;
    const int exitCode = runPowerShell(
        QStringLiteral("Get-AppxPackage | Where-Object { $_.PackageFullName -like 'Microsoft.MinecraftUWP_*' -or "
                       "$_.PackageFullName -like 'Microsoft.MinecraftWindowsBeta_*' } | Select-Object -First 1 -ExpandProperty PackageFullName"),
        &standardOutput, &standardError, 60 * 1000);
    if (exitCode != 0) {
        qWarning() << "Could not enumerate the installed packages:" << standardError;
        return QString();
    }
    return standardOutput.section(QLatin1Char('\n'), 0, 0).trimmed();
#endif
}

QString installedPackageLocation(const QString& fullName)
{
#if defined(_MSC_VER)
    try {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        winrt::Windows::Management::Deployment::PackageManager manager;
        for (auto const& package : manager.FindPackagesForUser(winrt::hstring())) {
            if (QString::fromWCharArray(package.Id().FullName().c_str()) == fullName) {
                return QString::fromWCharArray(package.InstalledLocation().Path().c_str());
            }
        }
    } catch (const winrt::hresult_error& exception) {
        qWarning() << "Could not read the package location:" << QString::fromWCharArray(exception.message().c_str());
    }
    return QString();
#else
    QString standardOutput;
    QString standardError;
    const int exitCode = runPowerShell(
        QStringLiteral("Get-AppxPackage | Where-Object { $_.PackageFullName -eq %1 } | Select-Object -First 1 -ExpandProperty InstallLocation")
            .arg(psQuote(fullName)),
        &standardOutput, &standardError, 60 * 1000);
    if (exitCode != 0) {
        qWarning() << "Could not read the package location:" << standardError;
        return QString();
    }
    return standardOutput.section(QLatin1Char('\n'), 0, 0).trimmed();
#endif
}

QString aumidForPackage(const QString& fullName)
{
#if defined(_MSC_VER)
    try {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        winrt::Windows::Management::Deployment::PackageManager manager;
        for (auto const& package : manager.FindPackagesForUser(winrt::hstring())) {
            if (QString::fromWCharArray(package.Id().FullName().c_str()) != fullName) {
                continue;
            }
            const QString familyName = QString::fromWCharArray(package.Id().FamilyName().c_str());
            const QString installedPath = QString::fromWCharArray(package.InstalledLocation().Path().c_str());
            const QString applicationId = manifestApplicationId(FS::PathCombine(installedPath, QStringLiteral("AppxManifest.xml")));
            if (familyName.isEmpty() || applicationId.isEmpty()) {
                return QString();
            }
            return familyName + QStringLiteral("!") + applicationId;
        }
    } catch (const winrt::hresult_error& exception) {
        qWarning() << "Could not read the package family name:" << QString::fromWCharArray(exception.message().c_str());
    }
    return QString();
#else
    QString standardOutput;
    QString standardError;
    const int exitCode = runPowerShell(
        QStringLiteral("$p = Get-AppxPackage | Where-Object { $_.PackageFullName -eq %1 } | Select-Object -First 1; "
                       "if ($p) { $p.PackageFamilyName; $p.InstallLocation }")
            .arg(psQuote(fullName)),
        &standardOutput, &standardError, 60 * 1000);
    if (exitCode != 0) {
        qWarning() << "Could not read the package family name:" << standardError;
        return QString();
    }
    const QStringList lines = standardOutput.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    if (lines.size() < 2) {
        return QString();
    }
    const QString familyName = lines.at(0).trimmed();
    const QString installedPath = lines.at(1).trimmed();
    const QString applicationId = manifestApplicationId(FS::PathCombine(installedPath, QStringLiteral("AppxManifest.xml")));
    if (familyName.isEmpty() || applicationId.isEmpty()) {
        return QString();
    }
    return familyName + QStringLiteral("!") + applicationId;
#endif
}

qint64 activateAumid(const QString& aumid, const QString& arguments, QString* error)
{
    qint64 pid = -1;
    QString activationError;
    std::thread worker([&pid, &activationError, aumid, arguments]() {
        if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) {
            activationError = QObject::tr("Could not initialize COM for the game activation.");
            return;
        }
        /* Spelled out so MinGW needs neither __uuidof nor the uuid import library. */
        static const GUID kClsidApplicationActivationManager = {0x45BA127D, 0x10A8, 0x46EA, {0x8A, 0xB7, 0x56, 0xEA, 0x90, 0x78, 0x94, 0x3C}};
        static const GUID kIidApplicationActivationManager = {0x2E941141, 0x7F97, 0x4756, {0xBA, 0x1D, 0x9D, 0xEC, 0xDE, 0x89, 0x4A, 0x3D}};
        IApplicationActivationManager* activationManager = nullptr;
        HRESULT hr = CoCreateInstance(kClsidApplicationActivationManager,
                                      nullptr,
                                      CLSCTX_LOCAL_SERVER,
                                      kIidApplicationActivationManager,
                                      reinterpret_cast<void**>(&activationManager));
        if (FAILED(hr) || !activationManager) {
            activationError = QObject::tr("Could not create the application activation manager (error 0x%1).").arg(QString::number(hr, 16));
            CoUninitialize();
            return;
        }
        const std::wstring wideAumid = aumid.toStdWString();
        const std::wstring wideArguments = arguments.toStdWString();
        DWORD activatedPid = 0;
        hr = activationManager->ActivateApplication(wideAumid.c_str(), wideArguments.c_str(), AO_NOERRORUI, &activatedPid);
        activationManager->Release();
        CoUninitialize();
        if (FAILED(hr)) {
            activationError = QObject::tr("Activating the game failed (error 0x%1).").arg(QString::number(hr, 16));
            return;
        }
        pid = static_cast<qint64>(activatedPid);
    });
    worker.join();
    if (pid < 0 && error) {
        *error = activationError;
    }
    return pid;
}

bool activateProtocol(const QString& protocolUrl, QString* error)
{
    const HINSTANCE result = ShellExecuteW(nullptr, L"open", protocolUrl.toStdWString().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(result) <= 32) {
        if (error) {
            *error = QObject::tr("Opening %1 through its protocol handler failed.").arg(protocolUrl);
        }
        return false;
    }
    return true;
}

}  // namespace GdkPackageInstaller

#else  // !Q_OS_WIN

#include <QObject>

namespace GdkPackageInstaller {

bool isSupported()
{
    return false;
}

bool isRunningUnderWine()
{
    return false;
}

bool installPackageFile(const QString& packageFilePath, QString* error)
{
    Q_UNUSED(packageFilePath)
    if (error) {
        *error = QObject::tr("Windows package installation is only available on Windows.");
    }
    return false;
}

bool registerExtractedPackage(const QString& manifestPath, QString* error)
{
    Q_UNUSED(manifestPath)
    if (error) {
        *error = QObject::tr("Windows package registration is only available on Windows.");
    }
    return false;
}

QString installedMinecraftPackageFullName()
{
    return QString();
}

QString installedPackageLocation(const QString& fullName)
{
    Q_UNUSED(fullName)
    return QString();
}

QString aumidForPackage(const QString& fullName)
{
    Q_UNUSED(fullName)
    return QString();
}

qint64 activateAumid(const QString& aumid, const QString& arguments, QString* error)
{
    Q_UNUSED(aumid)
    Q_UNUSED(arguments)
    if (error) {
        *error = QObject::tr("Windows application activation is only available on Windows.");
    }
    return -1;
}

bool activateProtocol(const QString& protocolUrl, QString* error)
{
    Q_UNUSED(protocolUrl)
    if (error) {
        *error = QObject::tr("Protocol activation is only available on Windows.");
    }
    return false;
}

}  // namespace GdkPackageInstaller

#endif  // Q_OS_WIN
