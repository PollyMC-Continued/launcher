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

#include <QHash>
#include <QRandomGenerator>
#include <QString>
#include <chrono>

// The online skin and cape browsers start on a different part of the catalog every few minutes, so
// the feed is not the same on every visit. The change is only evaluated when a browser opens: if the
// feed is older than the interval the browser starts on a new page, otherwise it starts on the same
// page as the last visit. Intervals that pass while no browser is opened never stack up, so opening
// the browser after an hour is still just one change.
namespace FeedRotation {

constexpr std::chrono::minutes kInterval{ 5 };
constexpr int kMaxStartPage = 25;  // 20 skins per catalog page

struct State {
    std::chrono::steady_clock::time_point changed;
    int page = 1;
    bool initialized = false;
};

inline QHash<QString, State>& states()
{
    static QHash<QString, State> s_states;
    return s_states;
}

// the catalog page the named feed ("skins", "capes") starts on; call it when the browser opens
inline int startPage(const QString& feed)
{
    auto& state = states()[feed];
    const auto now = std::chrono::steady_clock::now();
    if (!state.initialized || now - state.changed >= kInterval) {
        int page = state.page;
        while (page == state.page)
            page = QRandomGenerator::global()->bounded(1, kMaxStartPage + 1);
        state.page = page;
        state.changed = now;
        state.initialized = true;
    }
    return state.page;
}

// the catalog has nothing on the page that was picked (it is past the end): use the first page instead
inline void fallBackToFirstPage(const QString& feed)
{
    states()[feed].page = 1;
}

}  // namespace FeedRotation
