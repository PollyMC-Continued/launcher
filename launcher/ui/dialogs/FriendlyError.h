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

#include <QString>
#include <QStringList>

namespace FriendlyError {

/// Kinds of failure that are almost never a launcher bug: the user's connection, security
/// software, the disk or the system clock. Real crashes and code bugs are deliberately absent -
/// those go through the normal report flow unchanged.
enum class Category {
    None,
    Network,            ///< host unreachable / timeouts / refused or dropped connections
    AntivirusPermission,  ///< "Permission denied", file locked by security software
    DiskFull,           ///< no space, read-only folder, path too long
    Ssl,                ///< certificate / handshake problems (clock, intercepting proxy)
    SystemShutdown,     ///< the previous run ended with a reboot or power loss (info only, no dialog)
};

struct Advice {
    QString title;        ///< short headline, e.g. "Can't reach example.com"
    QString explanation;  ///< plain-language note about what most likely went wrong
    QStringList steps;    ///< 2-4 concrete things to try
};

/// \brief Classifies an error text (as it ends up in failure windows, logs and report
/// descriptions) into one of the friendly categories.
///
/// \return Category::None when the text gives no clue - which keeps the normal error and
/// report flow, including the prominent report button.
Category categorize(const QString& errorText);

/// A short name for the category, used inside sentences like "This looks like a %1 problem".
QString problemName(Category category);

/// The friendly advice for a category: a title, a plain explanation and 2-4 fix steps.
/// \param host shown once instead of a URL list for Category::Network; ignored otherwise.
Advice adviceFor(Category category, const QString& host = QString());

}  // namespace FriendlyError
