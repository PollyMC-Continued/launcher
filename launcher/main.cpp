// SPDX-License-Identifier: GPL-3.0-only
/*
 *  PollyMC-Continued - Minecraft Launcher
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
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

#include <QDebug>
#include <QScreen>

#include <cstdio>
#include <iostream>

#include "Application.h"

#include "console/BootDebug.h"

#if defined Q_OS_WIN32
#include "console/WindowsConsole.h"
#endif

// Forwards every Qt message emitted before the Application installs its real
// logger (Application.cpp: qInstallMessageHandler(appDebugOutput)) into
// boot-debug.log. This is what captures fatal startup errors such as
// "Failed to load platform plugin 'windows'" verbatim — the kind of error
// that kills the process before the launcher's own log file is ever created.
static void bootMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    const char* tag = "qt-msg";
    switch (type) {
        case QtDebugMsg:
            tag = "qt-debug";
            break;
        case QtWarningMsg:
            tag = "qt-warning";
            break;
        case QtCriticalMsg:
            tag = "qt-critical";
            break;
        case QtFatalMsg:
            tag = "qt-FATAL";
            break;
        case QtInfoMsg:
        default:
            tag = "qt-info";
            break;
    }
    const QByteArray utf8 = message.toUtf8();
    char line[2100];
    if (type == QtFatalMsg && context.file) {
        snprintf(line, sizeof(line), "%s: %s (%s:%u)", tag, utf8.constData(), context.file, context.line);
    } else {
        snprintf(line, sizeof(line), "%s: %s", tag, utf8.constData());
    }
    line[sizeof(line) - 1] = '\0';
    BootDebug::line(line);
}

int main(int argc, char* argv[])
{
    // Ultra-early crash diagnostics: must run before anything else so that a
    // failure anywhere below (missing DLL, platform plugin, static init, ...)
    // still leaves a trace in boot-debug.log next to the executable.
    BootDebug::init();
    BootDebug::stage("main: entered");

#if defined Q_OS_WIN32
    // used on Windows to attach the standard IO streams
    console::WindowsConsoleGuard _consoleGuard;
    BootDebug::stage("main: console streams attached");
#endif

    // Qt6 enables high-DPI scaling automatically, but the default rounding
    // policy (PassThrough) produces muddled fractional scaling under KDE
    // Plasma. Round sanely unless the user (or Plasma) already set a policy
    // via QT_SCALE_FACTOR_ROUNDING_POLICY.
    // NOTE: only applies on Linux - Windows/macOS keep PassThrough so common
    // 125%/150% display scales render at their true size instead of being
    // floored to 100%.
#if defined Q_OS_LINUX
    if (qEnvironmentVariableIsEmpty("QT_SCALE_FACTOR_ROUNDING_POLICY"))
        QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::RoundPreferFloor);
#endif

    // Capture Qt startup messages (see bootMessageHandler above) until the
    // Application constructor replaces this handler with its real one.
    qInstallMessageHandler(bootMessageHandler);

    // initialize Qt
    Application app(argc, argv);
    BootDebug::stage("main: Application constructed");

    switch (app.status()) {
        case Application::StartingUp:
        case Application::Initialized: {
            Q_INIT_RESOURCE(multimc);
            Q_INIT_RESOURCE(backgrounds);
            Q_INIT_RESOURCE(documents);
            Q_INIT_RESOURCE(pollymc);

            Q_INIT_RESOURCE(pe_dark);
            Q_INIT_RESOURCE(pe_light);
            Q_INIT_RESOURCE(pe_blue);
            Q_INIT_RESOURCE(pe_colored);
            Q_INIT_RESOURCE(breeze_dark);
            Q_INIT_RESOURCE(breeze_light);
            Q_INIT_RESOURCE(OSX);
            Q_INIT_RESOURCE(iOS);
            Q_INIT_RESOURCE(flat);
            Q_INIT_RESOURCE(flat_white);
            Q_INIT_RESOURCE(branding);

            Q_INIT_RESOURCE(shaders);
            BootDebug::stage("main: entering event loop");
            for (auto* screen : app.screens()) {
                qDebug() << "HiDPI screen:" << screen->name() << "DPR:" << screen->devicePixelRatio()
                         << "geometry:" << screen->geometry() << "logical DPI:" << screen->logicalDotsPerInch();
            }
            return app.exec();
        }
        case Application::Failed:
            return 1;
        case Application::Succeeded:
            return 0;
        default:
            return -1;
    }
}
