# PollyMC-Continued

Lets you play Minecraft **without a Microsoft account** — add offline accounts and launch the full game with no restrictions.

## Features
- Offline accounts — no Microsoft login required
- No demo mode — offline accounts launch the full game
- Custom skins — save and load skins locally
- Offline skin agent — Java agent that serves local skins
- Setup wizard — offers offline account on first launch
- NSIS installer with upgrade support

## Download
[Latest release](https://github.com/corecommit/PollyMC-Continued/releases)

## Build

### Windows
```
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/c/msys64/mingw64 -S . -B build
cmake --build build -j4
cmake --install build --prefix C:/pollymc_build
makensis pollymc_installer.nsi
```
Requires: MSYS2 + MinGW-w64, Qt 6, CMake, Ninja, NSIS.

### Linux
```
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -S . -B build
cmake --build build -j$(nproc)
cmake --install build --prefix /tmp/pollymc
# Package as AppImage (optional)
./linuxdeploy-x86_64.AppImage --appdir appdir --plugin qt --output appimage
```
Requires: Qt 6, CMake, Ninja, extra-cmake-modules, tomlplusplus, cmark, qrencode, libarchive.

### macOS
```
brew install cmake ninja qt extra-cmake-modules cmark qrencode libarchive tomlplusplus
brew install --cask temurin@17
bash packaging/macos/build-local.sh
```
Output in `dist-macos/` — ready-to-run `.app`, ZIP, DMG.

## Commit conventions

The CI auto-creates a release on every push to `main`. Which part of the version bumps depends on commit messages since the last tag:

| Commit starts with | Bump | Example |
|---|---|---|
| `BREAKING CHANGE` (in body) | Major (`10.0.0`) | A breaking API change |
| `feat:` / `Add ` / `Build ` / `Enhance ` | Minor (`9.1.0`) | A new feature |
| anything else | Patch (`9.0.1`) | A bug fix or refactor |

Put `[skip release]` anywhere in a commit message to skip the release entirely (builds still run, no tag created).

## Contributors
- [equs](https://github.com/corecommit) — maintainer
- [Kiwi Araga](https://github.com/kiwiaraga2000) — contributor
- [gib-b](https://github.com/gib-b) — contributor

## Credits
[Prism Launcher](https://github.com/PrismLauncher/PrismLauncher) · [PolyMC](https://github.com/PolyMC/PolyMC) · [MultiMC](https://multimc.org)

## License
GPL-3.0. See [LICENSE](LICENSE).