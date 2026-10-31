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
#include <QRegularExpression>
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

// ---- version range evaluation ------------------------------------------------
// The game loaders reject a mod when a required dependency is outside the declared range
// (NeoForge: "Incompatible dependency detected"), so this is a real crash, not a theory.
// The evaluation is deliberately timid: anything that is not understood with certainty is
// "Unknown" and never reported, because a false accusation costs more trust than a missed hit.
enum class Verdict { Inside, Outside, Unknown };
enum class Cmp { Less, Equal, Greater, Unknown };

bool isAllDigits(const QString& s)
{
    if (s.isEmpty())
        return false;
    for (const QChar c : s) {
        if (!c.isDigit())
            return false;
    }
    return true;
}

QStringList splitVersion(const QString& s)
{
    QStringList out;
    QString cur;
    for (const QChar c : s) {
        if (c == QLatin1Char('.') || c == QLatin1Char('-')) {
            out.append(cur);
            cur.clear();
        } else {
            cur.append(c);
        }
    }
    out.append(cur);
    return out;
}

// Compare two versions segment by segment ('.' and '-' separated). All-numeric segments
// compare numerically ("21" > "4", which a plain string compare gets wrong). When the two
// differ only by a qualifier ("1.0.0" vs "1.0.0-beta"), the ordering is loader-specific,
// so we refuse to decide.
Cmp compareVersions(const QString& a, const QString& b)
{
    if (a.isEmpty() || b.isEmpty())
        return Cmp::Unknown;
    for (const QString& v : { a, b }) {
        if (v.contains(QLatin1Char('$')) || v.contains(QLatin1Char('{')) || v.contains(QLatin1Char(' ')))
            return Cmp::Unknown;  // unexpanded build placeholder and other junk
    }

    const QStringList sa = splitVersion(a);
    const QStringList sb = splitVersion(b);
    const int common = std::min(sa.size(), sb.size());

    bool aHasQualifier = false, bHasQualifier = false;
    for (const QString& t : sa)
        if (!isAllDigits(t))
            aHasQualifier = true;
    for (const QString& t : sb)
        if (!isAllDigits(t))
            bHasQualifier = true;
    for (const QStringList* list : { &sa, &sb }) {
        for (const QString& t : *list) {
            if (t.compare(QLatin1String("x"), Qt::CaseInsensitive) == 0 || t == QLatin1String("*") || t == QLatin1String("+"))
                return Cmp::Unknown;  // wildcard segment: not a concrete version
        }
    }

    for (int i = 0; i < common; ++i) {
        const QString& ta = sa.at(i);
        const QString& tb = sb.at(i);
        if (isAllDigits(ta) && isAllDigits(tb)) {
            if (ta.size() > 18 || tb.size() > 18)  // avoid overflow paranoia on silly input
                return Cmp::Unknown;
            const qulonglong na = ta.toULongLong(), nb = tb.toULongLong();
            if (na < nb)
                return Cmp::Less;
            if (na > nb)
                return Cmp::Greater;
            continue;
        }
        const int c = ta.compare(tb, Qt::CaseInsensitive);
        if (c < 0)
            return Cmp::Less;
        if (c > 0)
            return Cmp::Greater;
    }
    // the common prefix is equal
    if (sa.size() == sb.size())
        return Cmp::Equal;
    // one is longer: only safe to decide when both are plain numbers ("1.3" vs "1.3.1")
    if (!aHasQualifier && !bHasQualifier) {
        const QStringList& longer = sa.size() > sb.size() ? sa : sb;
        for (int i = common; i < longer.size(); ++i) {
            if (!isAllDigits(longer.at(i)))
                return Cmp::Unknown;
            if (longer.at(i).toULongLong() != 0)
                return sa.size() > sb.size() ? Cmp::Greater : Cmp::Less;
        }
        return Cmp::Equal;  // "1.3.0" == "1.3"
    }
    return Cmp::Unknown;
}

// A Maven range, as used by NeoForge/Forge ("[1.3.8,1.4)"). A single set only: anything
// fancier (multiple sets, wildcards) is Unknown, because a wrong "Outside" would be a false
// accusation while a skipped check is merely a missed one.
Verdict evaluateMavenRange(const QString& range, const QString& version)
{
    if (range.size() < 2)
        return Verdict::Unknown;
    const bool lowerInclusive = range.startsWith(QLatin1Char('['));
    const bool lowerExclusive = range.startsWith(QLatin1Char('('));
    const bool upperInclusive = range.endsWith(QLatin1Char(']'));
    const bool upperExclusive = range.endsWith(QLatin1Char(')'));
    if (!(lowerInclusive || lowerExclusive) || !(upperInclusive || upperExclusive))
        return Verdict::Unknown;

    const QString body = range.mid(1, range.size() - 2);
    if (body.contains(QLatin1Char('[')) || body.contains(QLatin1Char(']')) || body.contains(QLatin1Char('(')) ||
        body.contains(QLatin1Char(')')))
        return Verdict::Unknown;  // several sets in one spec - not worth the risk

    const int comma = body.indexOf(QLatin1Char(','));
    if (comma < 0)
        return Verdict::Unknown;
    const QString low = body.left(comma);
    const QString high = body.mid(comma + 1);

    if (!low.isEmpty()) {
        const Cmp c = compareVersions(version, low);
        if (c == Cmp::Unknown)
            return Verdict::Unknown;
        if (lowerInclusive && c == Cmp::Less)
            return Verdict::Outside;
        if (lowerExclusive && (c == Cmp::Less || c == Cmp::Equal))
            return Verdict::Outside;
    }
    if (!high.isEmpty()) {
        const Cmp c = compareVersions(version, high);
        if (c == Cmp::Unknown)
            return Verdict::Unknown;
        if (upperInclusive && c == Cmp::Greater)
            return Verdict::Outside;
        if (upperExclusive && (c == Cmp::Greater || c == Cmp::Equal))
            return Verdict::Outside;
    }
    return Verdict::Inside;
}

// Fabric/Quilt version predicates (">=1.3 <1.4", "^1.2", "~1.3"): space-separated
// conditions that must all hold. A bare version or an unsupported token is Unknown.
Verdict evaluatePredicate(const QString& predicate, const QString& version)
{
    if (predicate.isEmpty())
        return Verdict::Unknown;
    const QStringList tokens = predicate.split(QRegularExpression(QStringLiteral("[ \t]+")), Qt::SkipEmptyParts);
    bool anyChecked = false;
    for (const QString& token : tokens) {
        QString op, target;
        if (token.startsWith(QLatin1String(">="))) {
            op = QStringLiteral(">=");
            target = token.mid(2);
        } else if (token.startsWith(QLatin1String("<="))) {
            op = QStringLiteral("<=");
            target = token.mid(2);
        } else if (token.startsWith(QLatin1String("=="))) {
            op = QStringLiteral("==");
            target = token.mid(2);
        } else if (token.startsWith(QLatin1Char('>'))) {
            op = QStringLiteral(">");
            target = token.mid(1);
        } else if (token.startsWith(QLatin1Char('<'))) {
            op = QStringLiteral("<");
            target = token.mid(1);
        } else if (token.startsWith(QLatin1Char('~'))) {
            op = QStringLiteral("~");
            target = token.mid(1);
        } else if (token.startsWith(QLatin1Char('^'))) {
            op = QStringLiteral("^");
            target = token.mid(1);
        } else {
            return Verdict::Unknown;  // bare version token: exact match rules differ per loader
        }
        if (target.isEmpty())
            return Verdict::Unknown;

        if (op == QLatin1String("~") || op == QLatin1String("^")) {
            // ~1.2.3 == >=1.2.3 <1.3.0 ; ^1.2.3 == >=1.2.3 <2.0.0
            const Cmp lower = compareVersions(version, target);
            if (lower == Cmp::Unknown)
                return Verdict::Unknown;
            if (lower == Cmp::Less)
                return Verdict::Outside;
            const QStringList segments = splitVersion(target);
            if (segments.size() < 2 || !isAllDigits(segments.at(0)))
                return Verdict::Unknown;
            QString upper;
            if (op == QLatin1String("~")) {
                if (!isAllDigits(segments.at(1)))
                    return Verdict::Unknown;
                upper = segments.at(0) + QLatin1Char('.') + QString::number(segments.at(1).toULongLong() + 1) + QLatin1String(".0");
            } else {
                upper = QString::number(segments.at(0).toULongLong() + 1) + QLatin1String(".0.0");
            }
            const Cmp up = compareVersions(version, upper);
            if (up == Cmp::Unknown)
                return Verdict::Unknown;
            if (up == Cmp::Equal || up == Cmp::Greater)
                return Verdict::Outside;
            anyChecked = true;
            continue;
        }

        const Cmp c = compareVersions(version, target);
        if (c == Cmp::Unknown)
            return Verdict::Unknown;
        bool ok = false;
        if (op == QLatin1String(">="))
            ok = (c == Cmp::Greater || c == Cmp::Equal);
        else if (op == QLatin1String("<="))
            ok = (c == Cmp::Less || c == Cmp::Equal);
        else if (op == QLatin1String(">"))
            ok = (c == Cmp::Greater);
        else if (op == QLatin1String("<"))
            ok = (c == Cmp::Less);
        else if (op == QLatin1String("=="))
            ok = (c == Cmp::Equal);
        if (!ok)
            return Verdict::Outside;
        anyChecked = true;
    }
    return anyChecked ? Verdict::Inside : Verdict::Unknown;
}

Verdict evaluateRange(QString range, const QString& version)
{
    if (range.isEmpty() || version.isEmpty())
        return Verdict::Unknown;
    range = range.trimmed();
    if (range.isEmpty() || range == QLatin1String("*") || range.compare(QLatin1String("x"), Qt::CaseInsensitive) == 0 ||
        range == QLatin1String("[0,)") || range == QLatin1String("[1,)") || range == QLatin1String("(,)"))
        return Verdict::Unknown;  // "any version" - nothing to check
    if (range.startsWith(QLatin1Char('[')) || range.startsWith(QLatin1Char('(')))
        return evaluateMavenRange(range, version);
    if (range.contains(QLatin1Char('<')) || range.contains(QLatin1Char('>')) || range.contains(QLatin1Char('=')) ||
        range.contains(QLatin1Char('~')) || range.contains(QLatin1Char('^')))
        return evaluatePredicate(range, version);
    return Verdict::Unknown;  // bare version = soft requirement in Maven terms
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
        // ids shipped inside an enabled jar (Jar-in-Jar): these are present at runtime even though
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

    // ---- 4. required dependencies whose installed version is outside the declared range ----
    // "requires buildinggadgets2 [1.3.8,1.4)" with buildinggadgets2 21.1.1 installed: the
    // dependency is present, so check 3 is happy, but the game still refuses to start.
    // A dependency can also be satisfied by a jar bundled inside another mod (Jar-in-Jar), whose
    // version we know from the nested jar's own metadata; that version has to fit the range too.
    // Only clear-cut cases are reported; anything the evaluator cannot decide is skipped.
    {
        // which enabled mod bundles which dependency id, for the report text
        QHash<QString, Mod*> bundlerOf;
        for (Mod* mod : enabled) {
            for (const QString& id : mod->bundledModIds()) {
                if (!bundlerOf.contains(id))
                    bundlerOf.insert(id, mod);
            }
        }

        for (Mod* mod : enabled) {
            const QHash<QString, QString> ranges = mod->dependencyRanges();
            for (auto it = ranges.constBegin(); it != ranges.constEnd(); ++it) {
                const QString& depId = it.key();
                const QString& range = it.value();
                if (depId.isEmpty() || depId == mod->mod_id() || ignoredDependencyIds().contains(depId))
                    continue;

                const QList<Mod*> targets = enabledById.value(depId);
                if (targets.isEmpty()) {
                    // not a file in the mods folder: maybe bundled inside another enabled jar
                    Mod* bundler = bundlerOf.value(depId, nullptr);
                    if (!bundler)
                        continue;  // not installed at all - that is check 3's business
                    const QString bundledVersion = bundler->bundledModVersions().value(depId);
                    if (bundledVersion.isEmpty())
                        continue;  // present, but the nested jar's version is unknown: nothing to check
                    if (evaluateRange(range, bundledVersion) != Verdict::Outside)
                        continue;

                    conflicts.append({ Kind::DependencyVersion, Severity::Error,
                                       tr("Outdated bundled dependency version: %1").arg(depId),
                                       tr("%1 (%2) requires %3 version %4, but the copy bundled inside %5 (%6) is version %7.\n"
                                          "The game will refuse to start with this combination.\n"
                                          "Update the mod that bundles it, or install a separate copy inside that range.")
                                           .arg(displayName(mod), fileLabel(mod), depId, range, displayName(bundler), fileLabel(bundler),
                                                bundledVersion),
                                       { mod, bundler } });
                    continue;
                }

                for (Mod* target : targets) {
                    if (target->version().isEmpty())
                        continue;
                    if (evaluateRange(range, target->version()) != Verdict::Outside)
                        continue;

                    conflicts.append({ Kind::DependencyVersion, Severity::Error,
                                       tr("Outdated dependency version: %1").arg(displayName(target)),
                                       tr("%1 (%2) requires %3 version %4, but %5 (%6) is installed.\n"
                                          "The game will refuse to start with this combination.\n"
                                          "Update or replace the dependency with a version inside that range.")
                                           .arg(displayName(mod), fileLabel(mod), depId, range, target->version(), fileLabel(target)),
                                       { mod, target } });
                }
            }
        }
    }

    return conflicts;
}
