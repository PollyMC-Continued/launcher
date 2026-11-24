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

#include "EnsureSoftwareRenderer.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>

#include "Application.h"
#include "SoftwareRenderer.h"
#include "net/ChecksumValidator.h"
#include "net/Download.h"

EnsureSoftwareRenderer::EnsureSoftwareRenderer(LaunchTask* parent, MinecraftInstance* instance) : LaunchStep(parent), m_instance(instance) {}

void EnsureSoftwareRenderer::executeTask()
{
#ifdef Q_OS_WIN
    if (!m_instance->settings()->get("SoftwareRendering").toBool() || SoftwareRenderer::isInstalled()) {
        emitSucceeded();
        return;
    }

    if (!SoftwareRenderer::canDownload()) {
        emit logLine(tr("Software rendering is turned on, but the software renderer is not installed and cannot be downloaded by this "
                        "version.\nCopy the 64-bit Mesa files opengl32.dll, libgallium_wgl.dll and libglapi.dll into this folder, or turn "
                        "software rendering off:\n  %1")
                         .arg(QDir::toNativeSeparators(SoftwareRenderer::folder())),
                     MessageLevel::Fatal);
        emitFailed(tr("The software renderer is not installed"));
        return;
    }

    QDir().mkpath(SoftwareRenderer::folder());
    emit logLine(tr("Downloading the software renderer (needed once)..."), MessageLevel::Launcher);

    m_job.reset(new NetJob(tr("Software renderer"), APPLICATION->network()));
    auto download = Net::Download::makeFile(SoftwareRenderer::downloadUrl(), SoftwareRenderer::archivePath());
    download->addValidator(new Net::ChecksumValidator(QCryptographicHash::Sha256, SoftwareRenderer::downloadSha256()));
    m_job->addNetAction(download);

    connect(m_job.get(), &Task::progress, this, &EnsureSoftwareRenderer::setProgress);
    connect(m_job.get(), &Task::status, this, &EnsureSoftwareRenderer::setStatus);
    connect(m_job.get(), &Task::failed, this, [this](QString reason) {
        QFile::remove(SoftwareRenderer::archivePath());
        emit logLine(tr("Could not download the software renderer: %1\nTurn software rendering off or try again with a working Internet "
                        "connection.")
                         .arg(reason),
                     MessageLevel::Fatal);
        emitFailed(reason);
    });
    connect(m_job.get(), &Task::succeeded, this, [this] {
        QString error;
        const bool ok = SoftwareRenderer::extract(SoftwareRenderer::archivePath(), error);
        QFile::remove(SoftwareRenderer::archivePath());
        if (!ok) {
            emit logLine(tr("Could not install the software renderer: %1").arg(error), MessageLevel::Fatal);
            emitFailed(error);
            return;
        }
        emit logLine(tr("Software renderer installed."), MessageLevel::Launcher);
        emitSucceeded();
    });
    m_job->start();
#else
    emitSucceeded();
#endif
}
