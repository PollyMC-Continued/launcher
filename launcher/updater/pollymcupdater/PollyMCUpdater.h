// SPDX-FileCopyrightText: 2023 Rachel Powers <508861+Ryex@users.noreply.github.com>
//
// SPDX-License-Identifier: GPL-3.0-only

/*
 *  PollyMC-Continued - Minecraft Launcher
 *  Copyright (C) 2023 Rachel Powers <508861+Ryex@users.noreply.github.com>
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
 */

#pragma once

#include <QtCore>

#include <QApplication>
#include <QDataStream>
#include <QDateTime>
#include <QDebug>
#include <QFlag>
#include <QIcon>
#include <QLocalSocket>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>
#include <memory>
#include <optional>

#include "QObjectPtr.h"
#include "net/Download.h"

#define POLLYMC_EXTERNAL_EXE
#include "FileSystem.h"

#include "GitHubRelease.h"

class PollyMCUpdaterApp : public QApplication {
    Q_OBJECT
   public:
    enum Status { Starting, Failed, Succeeded, Initialized, Aborted };
    PollyMCUpdaterApp(int& argc, char** argv);
    virtual ~PollyMCUpdaterApp();
    void loadReleaseList();
    void run();
    Status status() const { return m_status; }

   private:
    void fail(const QString& reason);
    void abort(const QString& reason);
    void showFatalErrorMessage(const QString& title, const QString& content);

    bool loadPollyMCVersionFromExe(const QString& exe_path);

    void downloadReleasePage(const QString& api_url, int page);
    int parseReleasePage(const QByteArray* response);

    bool needUpdate(const GitHubRelease& release);

    GitHubRelease getLatestRelease();
    GitHubRelease selectRelease();
    QList<GitHubRelease> newerReleases();
    QList<GitHubRelease> nonDraftReleases();

    void printReleases();

    QList<GitHubReleaseAsset> validReleaseArtifacts(const GitHubRelease& release);
    GitHubReleaseAsset selectAsset(const QList<GitHubReleaseAsset>& assets);
    void performUpdate(const GitHubRelease& release);
    void performInstall(QFileInfo file);
    void unpackAndInstall(QFileInfo file);
    void backupAppDir();
    std::optional<QDir> unpackArchive(QFileInfo file);

    QFileInfo downloadAsset(const GitHubReleaseAsset& asset);
    bool callAppImageUpdate();

    void moveAndFinishUpdate(QDir target);

   public slots:
    void downloadError(QString reason);

   private:
    const QString& root() { return m_rootPath; }

    bool isPortable() { return m_isPortable; }

    void clearUpdateLog();
    void logUpdate(const QString& msg);

    QString m_rootPath;
    QString m_dataPath;
    bool m_isPortable = false;
    bool m_isAppimage = false;
    bool m_isFlatpak = false;
    QString m_appimagePath;
    QString m_pollymcExecutable;
    QUrl m_pollymcRepoUrl;
    Version m_userSelectedVersion;
    bool m_checkOnly;
    bool m_showLatest = false;
    bool m_forceUpdate;
    bool m_printOnly;
    bool m_selectUI;
    bool m_allowDowngrade;
    bool m_allowPreRelease;
    bool m_silent = false;

    QString m_updateLogPath;

    QString m_pollymcBinaryName;
    QString m_pollymcVersion;
    int m_pollymcVersionMajor = -1;
    int m_pollymcVersionMinor = -1;
    int m_pollymcVersionPatch = -1;
    QString m_pollymcVersionChannel;
    QString m_pollymcGitCommit;

    GitHubRelease m_install_release;

    Status m_status = Status::Starting;
    std::unique_ptr<QNetworkAccessManager> m_network;
    QString m_current_url;
    Task::Ptr m_current_task;
    QList<GitHubRelease> m_releases;

   public:
    std::unique_ptr<QFile> logFile;
    bool logToConsole = false;

#if defined Q_OS_WIN32
    // used on Windows to attach the standard IO streams
    bool consoleAttached = false;
#endif
};
