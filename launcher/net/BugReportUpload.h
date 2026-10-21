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

#include "net/ByteArraySink.h"
#include "net/NetRequest.h"

#include <QByteArray>
#include <QString>
#include <QUrl>

/// Sends one bug report (JSON) to the report endpoint, which forwards it to the team's Discord.
class BugReportUpload : public Net::NetRequest {
   public:
    class Sink : public Net::ByteArraySink {
       public:
        Sink() = default;
        virtual ~Sink() = default;

        auto finalize(QNetworkReply& reply) -> Task::State override;
    };

    BugReportUpload(const QUrl& endpoint, const QByteArray& json);
    virtual ~BugReportUpload() = default;

   private:
    QNetworkReply* getReply(QNetworkRequest&) override;

    QByteArray m_json;
};
