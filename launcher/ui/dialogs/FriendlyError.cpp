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

#include "FriendlyError.h"

#include <QCoreApplication>
#include <QStringView>
#include <algorithm>
#include <initializer_list>

// No Q_OBJECT in this namespace: the strings go through QCoreApplication::translate with a
// fixed context, which lands in the same .ts files as every tr() call.
#define FTR(source) QCoreApplication::translate("FriendlyError", source)

namespace {
// Lowercased: matched against a lowercased text with QString::contains.
bool containsAny(const QString& lowerText, std::initializer_list<QStringView> patterns)
{
    return std::any_of(patterns.begin(), patterns.end(), [&lowerText](QStringView pattern) { return lowerText.contains(pattern); });
}
}  // namespace

namespace FriendlyError {

Category categorize(const QString& errorText)
{
    const QString text = errorText.toLower();

    // Most specific words first: an error that mentions certificates is an SSL problem even if
    // the connection also timed out, and a full disk is a disk problem even if a URL is quoted.
    if (containsAny(text, { u"ssl", u"tls", u"certificate", u"handshake failed" })) {
        return Category::Ssl;
    }
    if (containsAny(text, { u"no space", u"disk full", u"disk quota", u"read-only", u"read only", u"too long" })) {
        return Category::DiskFull;
    }
    if (containsAny(text,
                    { u"denied",
                      u"access is denied",
                      u"permission",
                      u"antivirus",
                      u"ransomware",
                      u"locking this file",
                      u"locked" })) {
        return Category::AntivirusPermission;
    }
    if (containsAny(text,
                    { u"timed out",
                      u"timeout",
                      u"connection refused",
                      u"host not found",
                      u"connection closed",
                      u"connection reset",
                      u"connection timed out",
                      u"network session failed",
                      u"temporary network failure",
                      u"operation canceled",
                      u"proxy connection refused",
                      u"server is down" })) {
        return Category::Network;
    }
    return Category::None;
}

QString problemName(Category category)
{
    switch (category) {
        case Category::Network:
            return FTR("connection");
        case Category::AntivirusPermission:
            return FTR("security software or folder permission");
        case Category::DiskFull:
            return FTR("disk space or file path");
        case Category::Ssl:
            return FTR("secure connection");
        case Category::SystemShutdown:
            return FTR("system shutdown");
        case Category::None:
            break;
    }
    return FTR("system");
}

Advice adviceFor(Category category, const QString& host)
{
    Advice advice;
    switch (category) {
        case Category::Network: {
            const QString shownHost = host.isEmpty() ? FTR("the server") : host;
            advice.title = FTR("Can't reach %1").arg(shownHost);
            advice.explanation = FTR("This is usually your internet connection, a firewall or antivirus program, a provider "
                                     "or regional block, or the server being down. It is not a problem with the launcher.");
            advice.steps = {
                FTR("Try another network (a phone hotspot works well) or a VPN."),
                FTR("Check your proxy settings in Settings -> Proxy."),
                FTR("Temporarily turn off antivirus or firewall filtering and try again."),
                FTR("Open https://%1 in a browser: if that fails too, the problem is on the way there, not in the launcher.").arg(host),
            };
            break;
        }
        case Category::AntivirusPermission:
            advice.title = FTR("A security program or folder permissions stopped the launcher");
            advice.explanation = FTR("Another program - often antivirus software, ransomware protection or a cloud-sync client - "
                                     "or the folder's permissions stopped a file from being written.");
            advice.steps = {
                FTR("Add the launcher and its data folder to your antivirus exclusions."),
                FTR("Check that the folder is not read-only and your user account can write to it."),
                FTR("Close other programs that may keep the file open, then retry."),
            };
            break;
        case Category::DiskFull:
            advice.title = FTR("Not enough disk space, or a path that is too long");
            advice.explanation = FTR("The disk is full, the destination folder is read-only, or a file path is too long for the "
                                     "system (on Windows, roughly 260 characters is the limit).");
            advice.steps = {
                FTR("Free up space on the drive and try again."),
                FTR("Check that the destination folder is writable."),
                FTR("Move the launcher or the instance to a folder with a shorter path."),
            };
            break;
        case Category::Ssl:
            advice.title = FTR("Secure connection problem");
            advice.explanation = FTR("The launcher could not verify a server's identity. The most common causes are a wrong "
                                     "system clock and security software or a proxy that intercepts connections.");
            advice.steps = {
                FTR("Check your computer's date and time."),
                FTR("Temporarily turn off HTTPS scanning in your antivirus or proxy."),
                FTR("Try another network or a VPN."),
            };
            break;
        case Category::SystemShutdown:
            advice.title = FTR("The launcher closed because the computer was shut down");
            advice.explanation = FTR("The previous session ended with a system shutdown, restart or power loss. That is not a "
                                     "problem with the launcher, so there is nothing to report.");
            break;
        case Category::None:
            break;
    }
    return advice;
}

}  // namespace FriendlyError
