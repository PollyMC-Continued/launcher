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

#include <QObject>

#include "LicensingClient.h"

/*
 * The Xodus implementation of LicensingClient (Linux).
 *
 * Xodus (xodus-gaming/xodus, GPL-3) signs in with the user's own Microsoft
 * account, obtains the title license and downloads / decrypts the MSIXVC
 * package from the official Xbox CDN; BedrockOnLinux's pinned build of it is
 * what the launcher fetches. The process boundary stays intact: the launcher
 * runs the binary as a subprocess and never links it, and the binary gets an
 * isolated HOME under the launcher's data directory so its token keyring
 * persists (and never touches the user's home) - a lost keyring would
 * provision a new Microsoft device, and an account only holds ten.
 */
class XodusLicensingClient : public QObject, public LicensingClient {
    Q_OBJECT
   public:
    /* The application-wide client (parented to the application object). */
    static XodusLicensingClient* get();

    ////// LicensingClient //////

    QString id() const override;
    QString displayName() const override;
    QString resolveBinary(const QString& userOverride, QString* reason) override;
    bool hasManagedCopy() const override;
    Task::Ptr ensureManagedCopy() override;

    /* The isolated HOME the client runs with (its keyring lives there). */
    static QString xodusHomePath();

   private:
    explicit XodusLicensingClient(QObject* parent = nullptr);
};
