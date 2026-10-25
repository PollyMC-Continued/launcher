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

/// Ultra-early boot and crash diagnostics.
///
/// Rationale: the launcher's own log file (`logs/PollyMC-0.log`) is only opened
/// fairly late in the Application constructor. Anything that dies before that
/// point — a missing/broken Qt DLL, a platform plugin that fails to load, a
/// `qFatal` during QApplication construction, a crash in static initializers —
/// leaves *no log at all*, which makes "crashes on start" reports (Winlator,
/// fresh machines, broken antivirus setups) nearly impossible to diagnose.
///
/// BootDebug closes that gap:
///   * `init()` is called as the very first statement of `main()`, before any
///     Qt object exists. It opens `boot-debug.log` **next to the executable**
///     (fallback: `%TEMP%`, then the working directory) using raw Win32 calls,
///     so it keeps working even when the Qt runtime itself is the problem.
///   * It installs crash handlers (SEH, abort/qFatal, std::terminate, CRT
///     invalid parameter, pure virtual call) that append the exception code,
///     the faulting address, the module it happened in, and the last startup
///     stage that was reached.
///   * `stage("...")` markers are sprinkled through the early startup path so
///     a dead run tells us exactly how far it got.
///
/// The file is appended to (never blocking startup on failure), rotated at
/// 256 KiB, mirrored to stderr when a console is attached, and every write is
/// flushed immediately so even hard aborts leave the last lines on disk.
///
/// Intentionally free of any Qt dependency.
namespace BootDebug {
/// Call as the first statement of `main()`. Safe to call only once.
void init();

/// Record a reached checkpoint, e.g. BootDebug::stage("app: data dir ready").
/// Also remembered as context for the next crash report.
void stage(const char* name);

/// Append a free-form diagnostic line (used by the early Qt message handler).
void line(const char* text);
}  // namespace BootDebug
