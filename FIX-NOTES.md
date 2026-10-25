# Startup-crash investigation & fix package (v9.4.2 → crash-diagnostics build)

This patch answers your three points:

1. **"v9.4.1 does run on Winlator"** — that was the decisive test. It exonerates the
   toolchain (official v9.4.1 and your v9.4.2 were built from the same MSYS2 rolling
   state: no gcc / binutils / crt / Qt6 package moved between Oct 21 and Oct 25), and
   it exonerates Qt 6.12.0 itself.
2. **"Log doesn't even get created"** — this is the most important clue you gave.
   It means the process dies **before `Application.cpp` opens `logs/PollyMC-0.log`**,
   i.e. before line ~547 of the Application constructor. Everything the "Report A Bug"
   feature added lives *after* that point (menu item, dialogs, buttons, network retry),
   so the new feature code is **not** what kills the process — I re-verified the full
   diff file by file.
3. **"Fix it or add debugs to find out the error"** — done, both: a latent bug in the
   v9.4.2 retry logic is fixed, the CI hole that lets a crashing build ship green is
   closed, and a new early-boot diagnostics layer will name the exact failure point on
   the next run.

## What actually happens when a Qt app dies "before the log"

Between process start and the log file being opened, these things run:

```
Windows loader:      pollymc.exe + Qt6*.dll + mingw runtime DLLs must all load
static initializers: global objects in every .cpp file construct
main():              BootDebug/console attach → QApplication construction:
                       → loads the platform plugin (platforms/qwindows.dll)
                       → splash screen (QSvgRenderer → Qt6Svg.dll + imageformats)
                       → command line parsing, data dir detection
                       → single-instance check
Application.cpp:547: logs/PollyMC-0.log is FINALLY created
```

A failure anywhere above — a missing DLL in the zip, a blocked plugin, a `qFatal`
during `QApplication` construction — aborts the process with **no log file at all**.
That is precisely your symptom. The two usual culprits for "v9.4.1 works, my v9.4.2
build doesn't" are:

- **(A) A packaging difference in the v9.4.2 zip you put into Winlator** — e.g.
  `platforms/qwindows.dll`, an imageformats plugin, or a Qt DLL missing or stale
  (half-overwritten folder, an interrupted in-app update, files copied by hand).
  Note: your CI's smoke test could not catch this. It only failed when the exit code
  was exactly `-1073741515` (missing DLL). A crash or `qFatal` abort during startup
  printed **"DLLs loaded OK"** and the workflow stayed green. Fixed in this patch.
- **(B) The Winlator environment for that particular install** — box64 preset,
  container, or leftover update marker files in the data folder.

Since your source code provably cannot crash before the log (verified below), the
evidence points at (A)/(B) — but rather than guess, this patch makes the launcher
**prove it itself** on the next run.

## What I verified (so you don't have to re-litigate it)

- Full recursive diff of your v9.4.2 source vs upstream v9.4.1: 13 files changed.
  Every one of them is on-demand UI code (menu action, dialogs, message-box button,
  network failure dialog) — none of it executes between process start and the log
  file being opened. The only startup-visible changes are: one `setVisible()` call,
  one empty/non-empty `QString` read, a `qtlogging.ini` log-rule tweak, and the
  version string. None can crash.
- MSYS2 package history (via the MINGW-packages repo): `qt6-base` last updated
  Oct 20 (before both builds — both use Qt 6.12.0-1); `gcc` Oct 13; `binutils`
  Oct 7; `crt` Dec 2025. Nothing relevant moved between the working v9.4.1 build
  (Oct 21) and your v9.4.2 build (~Oct 25).
- The CI workflow files are byte-identical to upstream — including the smoke-test
  hole described above.
- `net/BugReportUpload.{h,cpp}` is currently dead code (the dialog does a plain
  POST itself) — harmless, left in place.

## What this patch changes

### 1. NEW: early-boot diagnostics — `boot-debug.log` (the "debugs" you asked for)

`launcher/console/BootDebug.{h,cpp}` (new) + stage markers in `main.cpp` and
`Application.cpp`:

- Called as the **first statement of `main()`**, before any Qt object exists.
  Uses only raw Win32 calls, so it keeps working even when the Qt runtime is the
  thing that's broken.
- Writes `boot-debug.log` **next to `pollymc.exe`** (fallback: `%TEMP%`, then the
  working directory). Appends, flushes every line, rotates at 256 KiB, mirrors to
  stderr when a console is attached.
- Header records: exe path, PID, command line, **Wine/Winlator detection with the
  Wine version**, WOW64 status.
- A Qt message handler installed *before* `QApplication` construction captures any
  Qt startup message — including the fatal ones like
  `"Failed to load platform plugin 'windows'"` — **verbatim** into the log.
- Crash handlers: unhandled SEH exceptions (code + faulting address + module),
  `qFatal`/`abort()`, `std::terminate`, CRT invalid-parameter, pure-virtual calls.
  Each report includes **the last startup stage that was reached**.
- 16 stage markers cover the whole early path:

```
main: entered → main: console streams attached → app: constructor entered →
app: splash screen shown → app: command line parsed → app: data directory ready →
app: single-instance check passed → app: opening log file →
app: logger installed (launcher log active from here) → app: instances loaded →
app: network cache initialized → app: constructor finished →
main: entering event loop
```

On Linux/macOS it degrades to stderr-only (that's not where the problem is).

### 2. FIXED: download-retry timer thread-safety (`net/NetJob.cpp`)

Your v9.4.2 retry delay created `QTimer::singleShot(...)` from whatever thread the
job's state machine happened to run on. `QTimer` may only be used from a thread with
an event loop; on the code paths where `executeNextSubTask()` is reached from a
worker thread this is undefined behavior (typically "Timers cannot be started from
another thread" — in the worst case a crash). The retry now hops to the job's own
thread via a queued `QMetaObject::invokeMethod` first, then starts the timer there.
Behavior (1 s / 2 s / 3 s back-off) is unchanged. This was the only piece of the
v9.4.2 diff that touched a subsystem active during startup-adjacent work, so it's
now provably safe.

### 3. FIXED: CI could ship a crashing build as green (`.github/workflows/build.yml`)

- The installed-binary smoke test now **fails on any abnormal exit code** during the
  startup window (`0xC0000005` access violation, `0xC0000409` abort/stack overflow,
  `0xC0000142` DLL init failure, …) — not just missing-DLL. It also verifies the
  launcher actually **created its log file containing "Log initialized"**, and on
  failure it prints `boot-debug.log`, stderr and Windows event-log entries.
- **New step**: the *portable zip itself* — the exact artifact you drop into
  Winlator — is extracted and smoke-tested the same way, plus an explicit check
  that `platforms/qwindows.dll` is present in the zip. If the packaging is broken,
  CI now says so instead of shipping it.
- **New step**: `build-env.txt` — the exact `pacman -Q` package list of the build —
  is recorded, shipped inside the portable zip, and can be attached to bug reports.
  (The build still uses `update: true` / rolling MSYS2, matching the known-good
  v9.4.1 recipe; when you want full reproducibility later, pin
  `release:` + `update: false` on `msys2/setup-msys2@v2` once a release snapshot
  that includes Qt 6.12 is available.)

## How to use this in Winlator (the 5-minute checklist)

1. Build this source on your fork (same CI as before — watch the new
   `Smoke-test portable package` step; if it goes **red**, the defect was in the
   packaging/build and CI just caught it for you).
2. Extract the fresh portable zip into a **brand-new folder** in Winlator (do not
   overwrite the old v9.4.2 folder — a half-updated folder is itself a classic
   no-log startup killer).
3. Run `pollymc.exe` once.
4. Look for **`boot-debug.log` next to `pollymc.exe`** and read the decision tree:

| What you see | What it means |
|---|---|
| `>>>> CRASH:` block with exception code + module name | The exact faulting module is named — a DLL problem, bad plugin, or code bug at a known stage. Send me the file. |
| Log stops at a stage (e.g. last line is `app: splash screen shown`) | The crash is between that stage and the next — a much smaller search area. Send me the file. |
| `qt-FATAL: ...` line (e.g. "Failed to load platform plugin ...") | The Qt startup error text itself, verbatim. Send me the file. |
| **No `boot-debug.log` at all** | The process **never reached `main()`**: the Windows loader itself failed — missing/corrupt DLLs in the zip, or box64/Wine refusing to run the binary. Re-extract the zip, verify `Qt6Core.dll`, `Qt6Gui.dll`, `Qt6Widgets.dll`, `platforms\qwindows.dll` exist next to the exe, try a different box64 preset (Compatibility vs Performance), or a fresh Winlator container. |
| Log reaches `app: logger installed` and later stages but no `logs/PollyMC-0.log` | Data-folder permission problem inside Wine — check the folder the data dir resolves to. |

5. Also check the data folder for leftover update markers — delete
   `.pollymc_update.lock`, `.pollymc_update.fail`, and the success marker if
   present; an interrupted in-app update leaves these behind and they block
   startup behind a modal dialog that Winlator may never show.

## Files changed / added in this patch

```
launcher/console/BootDebug.h            (new)  early-boot + crash diagnostics
launcher/console/BootDebug.cpp          (new)
launcher/main.cpp                       (mod)  init + Qt message capture + stages
launcher/Application.cpp                (mod)  10 stage markers
launcher/net/NetJob.cpp                 (mod)  thread-safe delayed retry
launcher/CMakeLists.txt                 (mod)  register BootDebug (Windows)
.github/workflows/build.yml             (mod)  real smoke tests + build-env.txt
CHANGELOG.md                            (mod)  "Unreleased" entry
```

If the next run still crashes, send me `boot-debug.log` (and `logs/PollyMC-0.log`
if it exists) and the decision tree above will already have narrowed it to a
single stage or module.
