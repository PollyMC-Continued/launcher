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

#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include "BedrockVersion.h"

/*
 * The Microsoft Store / Windows Update delivery client for legacy UWP Bedrock
 * packages (the extract-and-register pipeline, versions up to 1.21.120.20).
 *
 * Verified facts this is built on (Phase 0 report, fetched live from
 * BedrockLauncher's StoreNetwork.cs and the MS-WUSP protocol documentation):
 *   - endpoints: https://fe3.delivery.mp.microsoft.com/ClientWebService/client.asmx
 *                https://fe3.delivery.mp.microsoft.com/ClientWebService/client.asmx/secured
 *   - action sequence: GetConfig -> GetCookie -> SyncUpdates -> GetExtendedUpdateInfo2
 *     (GetExtendedUpdateInfo2 returns the FileUrl fragments)
 *   - the Minecraft app category ids: release d25480ca-36aa-46e6-b76b-39608d49558c,
 *     preview 188f32fc-5eaa-45a8-9f78-7dde4322d131
 *   - user agent: Windows-Update-Agent/10.0.10011.16384 Client-Protocol/1.81
 *   - release channel downloads run without a user ticket
 *   - the download URLs point at tlu.dl.delivery.mp.microsoft.com and come
 *     wrapped with the file's SHA-1 digest
 *
 * DEFENSIVE IMPLEMENTATION NOTE (Rule 2): the SOAP envelope bodies below follow
 * the MS-WUSP shapes; they were not byte-compared against BedrockLauncher's
 * requests from inside this environment. The parsing is deliberately lenient
 * (namespace-agnostic element names) and every failure surfaces the exact
 * reason, so a protocol drift shows up as a clear error rather than a wrong
 * download. The manual test checklist (TESTING.md) includes verifying these
 * envelopes against BedrockLauncher's StoreNetwork.cs, which is GPL-3.
 */
class LegacyUwpFe3Source {
   public:
    /* One downloadable file of an update (URL + its SHA-1 digest, base64). */
    struct FileLocation {
        QString url;
        QString sha1Base64;
    };

    /*
     * Resolves the download locations of the update identified by any of the
     * given update ids (the version databases carry several). Returns the
     * locations; an empty list plus a reason in *error means nothing resolved.
     * Synchronous: call it from a task, not from the UI directly.
     */
    static QList<FileLocation> filesForUpdates(const QStringList& updateIds, BedrockVersion::Channel channel, QString* error);

   private:
    LegacyUwpFe3Source() = default;

    /* One SOAP request/response round against the fe3 endpoints. */
    static bool soapCall(const QString& endpoint, const QString& action, const QString& body, QByteArray* response, QString* error);

    static bool getConfig(QString* error);
    static bool getCookie(QString* cookie, QString* error);
    static bool syncUpdates(const QString& cookie, BedrockVersion::Channel channel, QList<QPair<QString, qint64>>& updates, QString* error);
    static bool getExtendedUpdateInfo2(const QString& cookie, qint64 revisionId, QList<FileLocation>& files, QString* error);

    /* The app category id matching the channel. */
    static QString appCategory(BedrockVersion::Channel channel);
};
