// SPDX-License-Identifier: GPL-3.0-only
/*
 *  PollyMC-Continued - Minecraft Launcher
 *  Copyright (C) 2023 flowln <flowlnlnln@gmail.com>
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
 *  Copyright (C) 2026 Trial97 <alexandru.tripon97@gmail.com>
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

#ifndef Q_MOC_RUN
#include <compare>
#endif
#include <cstdint>

#include <QDebug>
#include <QList>
#include <QString>
#include <QStringView>

// this implements the FlexVer
// https://git.sleeping.town/exa/FlexVer
//
// NOTE(backport): replaces the old Section/m_numPart/m_stringPart comparator,
// which violated strict weak ordering on real-world version strings
// ("b1.7.3" vs "1.6.4", "13w38a" vs "1.7", ...) and made std::sort read
// out of bounds (heap-buffer-overflow / access violation 0xC0000005 in
// Packwiz::V1::getIndexForMod while parsing .pw.toml metadata).
// Upstream fix: Prism Launcher commit 5a0931d ("fix heap overflow with
// unstable version comparation", Apr 3, 2026), first shipped in Prism 11.0.1.
class Version {
   public:
    Version(QString str) : m_string(std::move(str)) { parse(); }  // NOLINT(hicpp-explicit-conversions)
    Version() = default;

   private:
    struct Section {
        enum class Type : std::uint8_t { Null, Textual, Numeric, PreRelease };
        explicit Section(Type t = Type::Null, QString value = "") : t(t), value(std::move(value)) {}
        Type t;
        QString value;
        bool operator==(const Section& other) const = default;
        std::strong_ordering operator<=>(const Section& other) const;
    };

   private:
    void parse();

   public:
    QString toString() const { return m_string; }
    bool isEmpty() const { return m_string.isEmpty(); }

    friend QDebug operator<<(QDebug debug, const Version& v);

    bool operator==(const Version& other) const { return (*this <=> other) == std::strong_ordering::equal; }
    std::strong_ordering operator<=>(const Version& other) const;

   private:
    QString m_string;
    QList<Section> m_sections;
};
