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
#include <QStringList>

#include "BaseVersion.h"

/*
 * One entry of the Bedrock for Windows version list.
 *
 * A version knows which channel it belongs to (Release or Preview) and how its
 * Windows package is distributed: current versions ship as encrypted GDK
 * packages (.msixvc, handed to Windows for the entitled install), while legacy
 * versions up to the UWP era shipped as plain ZIP-format APPX files (.Appx,
 * extract-and-register pipeline). The packaging kind is derived from the file
 * name / URL extension the version databases provide - not from version-number
 * arithmetic - so boundary versions are classified by what their actual
 * package is.
 *
 * Field data comes from the community databases verified live in the Phase 0
 * report and re-verified while implementing this step: GdkLinks (package
 * URLs), Metadata (file names, SHA-256 hashes, update ids) and the Rayth
 * Network list (update ids; mirror only). Fields the merged sources do not
 * know stay empty; the consumers must treat them as optional.
 */
class BedrockVersion : public BaseVersion {
   public:
    enum class Channel { Release, Preview };
    enum class Packaging { Msixvc, Appx, Unknown };

    using Ptr = std::shared_ptr<BedrockVersion>;

    explicit BedrockVersion(const QString& versionId);

    ////// BaseVersion //////
    QString descriptor() const override;
    QString name() const override;
    QString typeString() const override;
    bool operator<(BaseVersion& other) const override;
    bool operator>(BaseVersion& other) const override;

    ////// identity //////
    const QString& versionId() const { return m_id; }

    ////// channel //////
    Channel channel() const { return m_channel; }
    void setChannel(Channel channel) { m_channel = channel; }
    static QString channelToString(Channel channel);

    ////// packaging (Windows install pipeline) //////
    Packaging packaging() const { return m_packaging; }
    void setPackaging(Packaging packaging) { m_packaging = packaging; }
    void setFileName(const QString& fileName);
    const QString& fileName() const { return m_fileName; }
    bool isLegacyUwp() const { return m_packaging == Packaging::Appx; }

    ////// package data (may be empty when the sources do not provide it) //////
    const QString& sha256() const { return m_sha256; }
    void setSha256(const QString& sha256) { m_sha256 = sha256; }
    bool hasSha256() const { return !m_sha256.isEmpty(); }

    const QStringList& packageUrls() const { return m_urls; }
    void setPackageUrls(const QStringList& urls) { m_urls = urls; }
    bool hasPackageUrls() const { return !m_urls.isEmpty(); }

    const QStringList& updateIds() const { return m_updateIds; }
    void setUpdateIds(const QStringList& updateIds) { m_updateIds = updateIds; }
    void addUpdateId(const QString& updateId);

    ////// comparisons //////
    /*
     * True when the dotted version number "candidate" is numerically newer than
     * "reference" ("1.21.120.20" vs "1.21.120.4": four-component, leading zeros
     * in a component do not matter).
     */
    static bool isNewerThan(const QString& candidate, const QString& reference);

    /*
     * Zero-padded form of the version number so that plain string comparison
     * matches numeric order.
     */
    QString sortKey() const;

   private:
    QString m_id;
    Channel m_channel = Channel::Release;
    Packaging m_packaging = Packaging::Unknown;
    QString m_fileName;
    QString m_sha256;
    QStringList m_urls;
    QStringList m_updateIds;
};
