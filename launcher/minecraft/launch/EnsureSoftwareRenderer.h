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

#pragma once

#include "launch/LaunchStep.h"
#include "minecraft/MinecraftInstance.h"
#include "net/NetJob.h"

/// Makes sure the Mesa software renderer is on disk when the instance asks for it (Windows only).
class EnsureSoftwareRenderer : public LaunchStep {
    Q_OBJECT

   public:
    explicit EnsureSoftwareRenderer(LaunchTask* parent, MinecraftInstance* instance);
    ~EnsureSoftwareRenderer() override = default;

    void executeTask() override;
    bool canAbort() const override { return false; }

   private:
    MinecraftInstance* m_instance;
    NetJob::Ptr m_job;
};
