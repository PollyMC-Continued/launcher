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

#include "ModConflictChecker.h"

#include <QHash>
#include <QSet>
#include <QStringList>
#include <algorithm>

#include "minecraft/mod/Mod.h"
#include "modplatform/ModIndex.h"
#include "modplatform/packwiz/Packwiz.h"

namespace {
// things that show up in "depends" but are not mods in the mods folder
const QSet<QString>& ignoredDependencyIds()
{
    static const QSet<QString> ids = { "minecraft", "java", "forge", "neoforge", "fabricloader", "fabric-loader", "quilt_loader" };
    return ids;
}

QString fileLabel(const Mod* mod)
{
    return mod->fileinfo().fileName();
}

QString displayName(const Mod* mod)
{
    const QString name = mod->name();
    return name.isEmpty() ? mod->mod_id() : name;
}

QString projectKey(const QString& id, ModPlatform::ResourceProvider provider)
{
    return id + QChar(0x1f) + QString::number(static_cast<int>(provider));
}
}  // namespace

QList<ModConflictChecker::Conflict> ModConflictChecker::check(const QList<Mod*>& mods)
{
    QList<Conflict> conflicts;

    // index everything that has a readable mod id (jars we could not parse are the health check's business)
    QHash<QString, QList<Mod*>> enabledById;
    QHash<QString, QList<Mod*>> disabledById;
    QList<Mod*> enabled;
    for (Mod* mod : mods) {
        if (!mod || !mod->valid())
            continue;
        if (mod->enabled()) {
            enabledById[mod->mod_id()].append(mod);
            enabled.append(mod);
        } else {
            disabledById[mod->mod_id()].append(mod);
        }
    }
    // a stable order keeps the report from reshuffling between runs
    auto byFile = [](const Mod* a, const Mod* b) { return a->fileinfo().fileName().toLower() < b->fileinfo().fileName().toLower(); };
    std::sort(enabled.begin(), enabled.end(), byFile);

    // ---- 1. the same mod id in more than one enabled file ----
    {
        QStringList ids = enabledById.keys();
        ids.sort(Qt::CaseInsensitive);
        for (const QString& id : ids) {
            auto group = enabledById.value(id);
            if (group.size() < 2)
                continue;
            std::sort(group.begin(), group.end(), byFile);

            QStringList lines;
            for (const Mod* mod : group) {
                const QString version = mod->version().isEmpty() ? tr("unknown version") : mod->version();
                lines << QStringLiteral("  • %1 (%2)").arg(fileLabel(mod), version);
            }
            conflicts.append({ Kind::DuplicateId, Severity::Error, tr("Duplicate mod: %1").arg(displayName(group.first())),
                               tr("These files all provide the mod id \"%1\", and the game will not start with more than one of them:\n"
                                  "%2\nKeep only one (usually the newest) and delete or disable the others.")
                                   .arg(id, lines.join('\n')),
                               group });
        }
    }

    // ---- 2. declared incompatibilities between two enabled mods ----
    {
        QHash<QString, Mod*> enabledByProject;
        for (Mod* mod : enabled) {
            if (mod->metadata())
                enabledByProject.insert(projectKey(mod->metadata()->project_id.toString(), mod->metadata()->provider), mod);
        }

        QSet<QString> reported;  // an incompatibility is often declared by both sides
        auto report = [&](Mod* declaring, Mod* other) {
            if (declaring == other)
                return;
            QString a = declaring->internal_id();
            QString b = other->internal_id();
            if (b < a)
                std::swap(a, b);
            if (reported.contains(a + QChar(0x1f) + b))
                return;
            reported.insert(a + QChar(0x1f) + b);

            conflicts.append({ Kind::Incompatible, Severity::Error,
                               tr("%1 conflicts with %2").arg(displayName(declaring), displayName(other)),
                               tr("%1 (%2) says it is incompatible with %3 (%4), and both are enabled.\n"
                                  "Disable or remove one of them.")
                                   .arg(displayName(declaring), fileLabel(declaring), displayName(other), fileLabel(other)),
                               { declaring, other } });
        };

        for (Mod* mod : enabled) {
            // from the jar itself (fabric.mod.json "breaks", quilt.mod.json "breaks", neoforge "incompatible")
            for (const QString& id : mod->breaks()) {
                for (Mod* other : enabledById.value(id))
                    report(mod, other);
            }
            // from the platform's dependency list (only available for mods installed through the launcher)
            if (mod->metadata()) {
                for (const auto& dep : mod->metadata()->dependencies) {
                    if (dep.type != ModPlatform::DependencyType::INCOMPATIBLE)
                        continue;
                    if (Mod* other = enabledByProject.value(projectKey(dep.addonId.toString(), mod->metadata()->provider), nullptr))
                        report(mod, other);
                }
            }
        }
    }

    // ---- 3. required dependencies that are disabled or absent ----
    {
        // ids shipped inside an enabled jar (NeoForge Jar-in-Jar): these are present at runtime even though
        // there is no file for them in the mods folder
        QSet<QString> bundledIds;
        for (const Mod* mod : enabled) {
            for (const QString& id : mod->bundledModIds())
                bundledIds.insert(id);
        }

        QHash<QString, QList<Mod*>> requiredBy;  // dependency id -> enabled mods that need it
        for (Mod* mod : enabled) {
            QSet<QString> seen;
            for (const QString& dep : mod->dependencies()) {
                if (dep.isEmpty() || dep == mod->mod_id() || ignoredDependencyIds().contains(dep) || seen.contains(dep))
                    continue;
                seen.insert(dep);
                if (!enabledById.contains(dep) && !bundledIds.contains(dep))
                    requiredBy[dep].append(mod);
            }
        }

        QStringList ids = requiredBy.keys();
        ids.sort(Qt::CaseInsensitive);
        for (const QString& id : ids) {
            const auto& needing = requiredBy.value(id);
            QStringList names;
            for (const Mod* mod : needing)
                names << displayName(mod);
            names.removeDuplicates();
            const QString needed = names.join(QStringLiteral(", "));

            if (disabledById.contains(id)) {
                const Mod* disabled = disabledById.value(id).first();
                conflicts.append({ Kind::DisabledDependency, Severity::Warning, tr("Disabled dependency: %1").arg(displayName(disabled)),
                                   tr("%1 needs the mod \"%2\", which is installed (%3) but disabled.\nEnable it to avoid a crash on startup.")
                                       .arg(needed, id, fileLabel(disabled)),
                                   needing });
            } else {
                conflicts.append({ Kind::MissingDependency, Severity::Warning, tr("Possibly missing dependency: %1").arg(id),
                                   tr("%1 needs a mod with the id \"%2\", but it was not found in the mods folder "
                                      "or bundled inside any of the installed mods.\n"
                                      "Download it, unless the game already starts fine.")
                                       .arg(needed, id),
                                   needing });
            }
        }
    }

    return conflicts;
}
