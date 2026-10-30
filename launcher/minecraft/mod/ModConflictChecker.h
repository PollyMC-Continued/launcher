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

#include <QCoreApplication>
#include <QList>
#include <QString>

class Mod;

/*
 * Looks at the mods of one instance and reports combinations that stop the game from starting
 * (or very likely will):
 *   - two enabled files that provide the same mod id,
 *   - an enabled mod that declares itself incompatible with another enabled mod
 *     (from the jar's own metadata and from the Modrinth/CurseForge dependency list),
 *   - an enabled mod whose required dependency is disabled or not present (ids bundled through
 *     NeoForge Jar-in-Jar inside an enabled mod count as present),
 *   - an enabled mod whose required dependency is present, but in a version outside the
 *     declared range ("requires buildinggadgets2 [1.3.8,1.4)" with 21.1.1 installed).
 *
 * Pure logic, no I/O: it only reads what the mod list already parsed. Disabled mods never cause conflicts.
 */
class ModConflictChecker {
    Q_DECLARE_TR_FUNCTIONS(ModConflictChecker)

   public:
    enum class Kind { DuplicateId, Incompatible, DisabledDependency, MissingDependency, DependencyVersion };
    enum class Severity { Warning, Error };

    struct Conflict {
        Kind kind;
        // Error: the game will not start. Warning: probably a problem, but it can be a false alarm
        // (a mod can provide a dependency in some way we do not read, e.g. a non-NeoForge loader's own bundling).
        Severity severity;
        QString title;
        QString detail;
        QList<Mod*> mods;  // the enabled mods that are involved
    };

    static QList<Conflict> check(const QList<Mod*>& mods);
};
