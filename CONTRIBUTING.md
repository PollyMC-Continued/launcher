# Contributing to PollyMC-Continued

Thanks for helping improve PollyMC-Continued! This project is a fork of [Prism Launcher](https://github.com/PrismLauncher/PrismLauncher), and many parts of its architecture follow Prism's [contributing guide](https://github.com/PrismLauncher/PrismLauncher/blob/develop/CONTRIBUTING.md). Use this guide for PollyMC-Continued-specific conventions, build instructions, and contribution workflow.

## Before you contribute

For pull requests, aim for one focused change at a time, target `main`, and rebase on the latest `main`. Include relevant tests and update the changelog for user-visible changes.

## Project map

| Path | What it contains | Contributor notes |
|---|---|---|
| `launcher/` | C++20 launcher using Qt 6 Widgets | Keep user-visible text translatable with `tr("...")`. Do not introduce QML. |
| `launcher/bot/`, `bot-server/` | Minecraft bot manager and Node.js server using `mineflayer` | The launcher starts `bot-server/` and communicates through newline-delimited JSON on stdin/stdout. `node` must be on `PATH` when the Bots window is used. |
| `libraries/launcher/` | Java launcher classes in `org.pollymc.*` | Update the explicit source lists in `libraries/launcher/CMakeLists.txt` when adding or removing Java files. |
| `program_info/` | Branding, icons, desktop entries, and metainfo | Keep platform-facing assets and metadata consistent with the project. |
| `translations/` | Qt Linguist `.ts` files | Preserve source strings, message contexts, and placeholders. |
| `buildconfig/`, `cmake/` | Build configuration | Keep changes compatible with the supported presets where practical. |

The Java launcher is built into `NewLaunch.jar`, `NewLaunchLegacy.jar` (legacy Minecraft versions below 1.6), and `SkinAgent.jar` (offline skin agent). The entry point must remain `org.pollymc.EntryPoint`, referenced from `launcher/minecraft/launch/LauncherPartLaunch.cpp`.

## Set up a build

### Prerequisites

- CMake **3.25 or newer**
- A C++20-compatible compiler
- Qt **6.4 or newer** with Core, CoreTools, Widgets, Concurrent, Network, Test, Xml, NetworkAuth, OpenGL, Svg, and WebSockets
- A JDK (1.7+ API level; Java launcher targets Java 8)
- vcpkg dependencies, including libarchive, tomlplusplus, cmark, libqrencode, and zlib

### Configure and build

Choose a preset from `CMakePresets.json`:

- `linux`
- `macos`
- `macos_universal`
- `windows_mingw`
- `windows_msvc`

For example, on Linux:

```bash
cmake --preset linux
cmake --build --preset linux
```

Build out of source; in-source builds are not supported. The macOS and Windows MSVC presets configure the vcpkg toolchain automatically. On Linux/BSD, you can supply system Qt and vcpkg dependencies yourself.

## Implementation conventions

### C++ and Qt

- Use C++20 and Qt 6 Widgets; do not add QML.
- Use the asynchronous task framework in `launcher/tasks/`: `Task`, `SequentialTask`, and `ConcurrentTask`.
- Use `NetJob` and `Download` in `launcher/net/` for network operations.
- Follow established Qt conventions, including `Q_OBJECT`, `signals:`/`slots:`, and `connect()` with member-function pointers where appropriate.
- Put user-facing strings inside `tr("...")` to support translation.
- Start each new source file with an SPDX and copyright header matching the repository's style:

```cpp
// SPDX-License-Identifier: GPL-3.0-only
/*
 *  PollyMC-Continued - Minecraft Launcher
 *  Copyright (C) <year> <Your Name> <your@email>
 *
 *  ...
 */
```

### Java launcher

- Use the `org.pollymc.*` package.
- Keep Java compilation compatible with `-target 8 -source 8`.
- Keep the entry point `org.pollymc.EntryPoint` unchanged.
- Java files are listed explicitly in `libraries/launcher/CMakeLists.txt`; update the relevant `SRC`, `LEGACY_SRC`, or `SKIN_AGENT_SRC` list whenever you add or remove a file. Do not rely on automatic globbing.

## Tests and smoke checks

Enable testing with `-DBUILD_TESTING=ON`, build the relevant targets, and run CTest:

```bash
ctest
```

For UI or launch changes, also build the relevant preset and check the feature manually:

- **General launcher changes:** create an offline account and an instance, then launch it.
- **Bot changes:** open the Bots window, verify a bot can join a server, and check that slash commands respond. Ensure `node` is available on `PATH`.
- **Java launcher or skin-agent changes:** verify that an offline account launches with the skin applied.

## Translations

Translation files are Qt Linguist `.ts` files in [`translations/`](translations/), one per language (such as `de.ts`, `fr.ts`, and `pt_BR.ts`).

To contribute a translation:

1. Fork the repository.
2. Open the relevant `.ts` file in [Qt Linguist](https://doc.qt.io/qt-6/qtlinguist-index.html), or edit its XML directly.
3. If the language file does not exist, run `lupdate` on the source tree, copy an existing `.ts` file and change its `language` attribute, or open an issue requesting the file.
4. Commit the changes and open a pull request against `main`.

Translation rules:

- Edit only `<translation>` contents. Do not modify `<source>` strings or message contexts.
- Keep placeholders such as `%1` unchanged.
- Leave untranslated strings empty or unchanged; English is used as the fallback.
- A message marked `<type>unfinished</type>` is not considered translated.

Once a translation pull request is merged, `.qm` files are compiled and published automatically. The launcher picks up updated translations on its next refresh.

## Commit messages, versions, and releases

CI calculates the release version from all commit messages since the previous tag. The highest applicable version bump wins.

| Rule | Version bump | Example |
|---|---|---|
| `BREAKING CHANGE` appears anywhere in the commit message | Major (`9.0.0` → `10.0.0`) | `feat: new API` with `BREAKING CHANGE: removes old config format` in the message |
| Subject starts with `feat`, `feature`, `Add`, `Build`, or `Enhance`, followed by `:`, `(`, or a space | Minor (`9.0.0` → `9.1.0`) | `feat: add drag-and-drop modpack import` |
| Any other subject, including `Fix:`, `Docs:`, `Chore:`, or no prefix | Patch (`9.0.0` → `9.0.1`) | `Fix: crash when loading offline skins` |

The matching is case-sensitive: `feat:` and `feature:` are lowercase; `Add:`, `Build:`, and `Enhance:` use the capitalization shown. CI considers every commit since the last tag, not just the latest one.

Special commit-message markers:

- `[skip-all]` — skip builds and the release entirely.
- `[skip release]` — run builds but skip the release.
- `[skip changelog]` — skip the changelog requirement when `CHANGELOG.md` is not being updated.

Unless `[skip changelog]` is used, the CI `changelog-check` job requires a `## vX.Y.Z` section in `CHANGELOG.md` matching the version calculated from commit messages. Changelog entries for user-visible changes become GitHub Release notes.

## Pull request checklist

Before opening a pull request, check that:

- [ ] The pull request targets `main` and covers one logical change.
- [ ] The branch is rebased on the latest `main`.
- [ ] Relevant tests pass, and UI or launch changes have been smoke-tested where practical.
- [ ] `CHANGELOG.md` has an entry for user-visible changes, or the commit message contains `[skip changelog]`.
- [ ] New build-time and runtime dependencies are documented in the pull request.
- [ ] New Java files are added to the correct source list in `libraries/launcher/CMakeLists.txt`.
- [ ] New user-visible strings use `tr("...")`.

CI builds Windows, Linux, and macOS artifacts and automatically publishes a pre-release. If a change is limited to documentation or CI and should not trigger builds or a release, use `[skip-all]` in a commit message.
