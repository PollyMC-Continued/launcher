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

#include <QString>

#include "tasks/Task.h"

/*
 * The boundary to the account-authenticated game licensing client (the Linux
 * side of Bedrock support).
 *
 * Phase 0 decision: the licensing client is an external GPL-3 program run as a
 * separate process (Xodus today), never linked into the launcher and never
 * bundled with it - the launcher only downloads a hash-pinned copy at runtime
 * or uses a binary the user points it at. A clean-room reimplementation of
 * the licensing protocol would slot in behind this same interface later.
 *
 * The client acquires the game package under the user's own Microsoft account
 * license: it owns the sign-in, the entitlement check and the decryption of
 * the encrypted package; the launcher deliberately has no part in that.
 */
class LicensingClient {
   public:
    virtual ~LicensingClient() = default;

    /* Stable id ("xodus"). */
    virtual QString id() const = 0;

    /* Human-readable name for logs and messages. */
    virtual QString displayName() const = 0;

    /*
     * Resolves the client binary. A user-supplied override path always wins;
     * then the managed, hash-verified copy. The empty string plus a reason in
     * *reason means the backend is not usable yet and the reason explains what
     * the user can do about it.
     */
    virtual QString resolveBinary(const QString& userOverride, QString* reason) = 0;

    /* True once the managed copy exists and its pinned hash verified. */
    virtual bool hasManagedCopy() const = 0;

    /*
     * Downloads and verifies the managed copy; the returned task succeeds
     * immediately when the copy is already in place. Returns nullptr when
     * the client only ever comes from the user's own configuration.
     */
    virtual Task::Ptr ensureManagedCopy() = 0;
};
