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
 *   - an enabled mod whose required dependency is disabled or not present.
 *
 * Pure logic, no I/O: it only reads what the mod list already parsed. Disabled mods never cause conflicts.
 */
class ModConflictChecker {
    Q_DECLARE_TR_FUNCTIONS(ModConflictChecker)

   public:
    enum class Kind { DuplicateId, Incompatible, DisabledDependency, MissingDependency };
    enum class Severity { Warning, Error };

    struct Conflict {
        Kind kind;
        // Error: the game will not start. Warning: probably a problem, but it can be a false alarm
        // (a dependency can be bundled inside another mod, which we do not look into).
        Severity severity;
        QString title;
        QString detail;
        QList<Mod*> mods;  // the enabled mods that are involved
    };

    static QList<Conflict> check(const QList<Mod*>& mods);
};
