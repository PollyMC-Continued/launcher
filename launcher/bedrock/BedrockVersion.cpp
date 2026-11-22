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

#include "BedrockVersion.h"

#include <QCoreApplication>
#include <QList>
#include <QStringList>
#include <QtGlobal>

namespace {
QList<qint64> parseNumericVersion(const QString& version)
{
    QList<qint64> parts;
    const QStringList segments = version.split('.', Qt::SkipEmptyParts);
    for (const QString& segment : segments) {
        bool ok = false;
        const qint64 value = segment.toLongLong(&ok);
        parts.append(ok ? value : 0);
    }
    return parts;
}
}  // namespace

BedrockVersion::BedrockVersion(const QString& versionId) : m_id(versionId) {}

QString BedrockVersion::descriptor() const
{
    return m_id;
}

QString BedrockVersion::name() const
{
    return m_id;
}

QString BedrockVersion::typeString() const
{
    /* Legacy UWP versions are shown as "Old", everything else by its channel. */
    if (m_packaging == Packaging::Appx) {
        return legacyLabel();
    }
    return channelToString(m_channel);
}

QString BedrockVersion::legacyLabel()
{
    return QCoreApplication::translate("BedrockVersion", "Old");
}

QString BedrockVersion::channelToString(Channel channel)
{
    switch (channel) {
        case Channel::Release:
            return QCoreApplication::translate("BedrockVersion", "Release");
        case Channel::Preview:
            return QCoreApplication::translate("BedrockVersion", "Preview");
    }
    return QString();
}

void BedrockVersion::setFileName(const QString& fileName)
{
    m_fileName = fileName;
    if (fileName.endsWith(".msixvc", Qt::CaseInsensitive)) {
        m_packaging = Packaging::Msixvc;
    } else if (fileName.endsWith(".appx", Qt::CaseInsensitive)) {
        m_packaging = Packaging::Appx;
    }
}

void BedrockVersion::addUpdateId(const QString& updateId)
{
    if (!updateId.isEmpty() && !m_updateIds.contains(updateId)) {
        m_updateIds.append(updateId);
    }
}

bool BedrockVersion::isNewerThan(const QString& candidate, const QString& reference)
{
    const QList<qint64> a = parseNumericVersion(candidate);
    const QList<qint64> b = parseNumericVersion(reference);
    const int segments = qMax(a.size(), b.size());
    for (int i = 0; i < segments; i++) {
        const qint64 x = i < a.size() ? a.at(i) : 0;
        const qint64 y = i < b.size() ? b.at(i) : 0;
        if (x != y) {
            return x > y;
        }
    }
    return false;
}

QString BedrockVersion::sortKey() const
{
    QStringList padded;
    const QList<qint64> parts = parseNumericVersion(m_id);
    for (const qint64 part : parts) {
        padded.append(QString("%1").arg(part, 8, 10, QChar('0')));
    }
    return padded.join('.');
}

bool BedrockVersion::operator<(BaseVersion& other) const
{
    auto bedrockOther = dynamic_cast<BedrockVersion*>(&other);
    if (bedrockOther) {
        return isNewerThan(bedrockOther->m_id, m_id);
    }
    return name() < other.name();
}

bool BedrockVersion::operator>(BaseVersion& other) const
{
    auto bedrockOther = dynamic_cast<BedrockVersion*>(&other);
    if (bedrockOther) {
        return isNewerThan(m_id, bedrockOther->m_id);
    }
    return name() > other.name();
}
