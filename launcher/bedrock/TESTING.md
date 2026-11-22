# Bedrock Edition — manual test checklist (Phase 1, Steps A–E)

This is the required manual test checklist for Bedrock Edition support (the code paths that
cannot be verified without real Windows/Linux machines, game ownership and network access).
Every item marked **[U]** is a *defensively implemented but unverified* item: the Phase 0
verification report proved the surrounding design, but the exact behavior could not be
checked from the development environment. Verify before shipping; if an item fails, the
launcher must surface a clear error (never a wrong download or a silent wrong behavior).

## 0. Build

- [ ] Windows: builds with MSVC (C++/WinRT headers, `winrt/Windows.Management.Deployment.h`).
      `GdkPackageInstaller.cpp` needs the Microsoft.Windows.CppWinRT headers — the
      `runtimeobject.lib` link pragma is already in the file. Verify the launcher's Windows
      build chain actually ships them (vcpkg/CI); if the toolchain is MinGW-only this file
      must be revisited (C++/WinRT headers require MSVC).
- [ ] Linux: builds clean; the whole `launcher/bedrock/` tree compiles (nothing there is
      Windows-only — the WinRT/COM code is behind `Q_OS_WIN`).
- [ ] macOS: builds; launching a Bedrock instance fails with the "no backend on this
      platform" message (expected — decision 6: no macOS support).

## 1. Java regression (rule 5: shared-code changes need explicit verification)

- [ ] Create, launch, update, copy, export, group, shortcut a **Java** instance; the shared
      changes (InstanceList, LaunchController, LaunchTask, InstancePageProvider,
      NewInstanceDialog, MainWindow, InstanceWindow, CreateShortcutDialog) must not change
      Java behavior:
  - [ ] Java launch still shows account selection, online/offline, server status steps.
  - [ ] `LaunchTask::instance()` still returns the MinecraftInstance for Java launches
        (it is a `dynamic_cast` now).
  - [ ] The New Instance dialog's Java pages (Custom / Import / Modrinth / CurseForge /
        ATLauncher / FTB / Technic) work unchanged; the Minecraft Edition switch defaults
        to Java.
  - [ ] Resource import into a Java instance still works; into a Bedrock instance it now
        says "only into a Minecraft (Java) instance".
- [ ] `instance.cfg` of old Java instances still loads (the `OneSix` branch of
      `InstanceList::loadInstance` is untouched).

## 2. Windows — GDK pipeline (current versions, .msixvc)

Prerequisites: Windows 10/11, a Microsoft account that owns *Minecraft for Windows* (or
Game Pass), Windows Gaming Services installed.

- [ ] Create a Bedrock instance with a current release (e.g. 1.26.x) with "Download the
      game files on creation" checked: the package downloads with progress, the SHA-256
      is verified (checksum validator + final file check), the file lands in
      `<dataRoot>/bedrock/packages/<versionId>/`.
- [ ] First launch / creation install: Windows shows the entitled install
      (AddPackageAsync); verify the install succeeds on an owning account and produces
      the package under the WindowsApps/XboxGames layout.
- [ ] **[U]** AddPackageAsync with a locally downloaded .msixvc: confirm Windows accepts
      the file without a Store-initiated download (BedrockLauncher does the same, but
      verify on a clean machine without a prior Store install of the game).
- [ ] **[U]** Behavior on an account that does **not** own the game: expect a deployment
      error; verify the launcher surfaces the HRESULT and Windows' own error text
      (nothing silently proceeds).
- [ ] Version-switch warning: install version A, then point the instance (or a second
      instance) at version B and launch: the dialog "Replace the installed Bedrock game?"
      must appear, No aborts the install with a clear message, Yes installs B.
- [ ] AUMID activation: the launch log shows the package full name, the AUMID
      (`<family>!<appId>` read from the installed AppxManifest.xml) and the activated PID;
      the game opens.
- [ ] **[U]** Data redirection: with "Keep this instance's game data separate" on:
      `%APPDATA%\Minecraft Bedrock` becomes a junction to the instance's `gamedata`
      folder; worlds/packs written by the game land in the instance; a second instance
      gets its own empty data on its launch (the junction is re-pointed). Verify:
      the junction survives a game restart; the adoption dialog on first redirect;
      the `.pre-instance` backup when the game recreates its folder after a version
      switch. **Back up the user's worlds before this test.**
- [ ] With the setting off: the game's default data folder is used and a log line says so.
- [ ] Optional helper: point "GDK launch helper DLL" at BedrockLauncher's
      `gamelaunchhelper.dll`: on the next launch it is copied next to the installed game
      and the exact version stays pinned (no auto-update). **[U]** verify the DLL placement
      path (`<installLocation>/gamelaunchhelper.dll`) matches a real BedrockLauncher
      deployment.
- [ ] Version marker: `<instance>/game/.bedrock_install` records version/packaging/sha256;
      relaunching skips the install when the installed package matches.

## 3. Windows — legacy UWP pipeline (versions ≤ 1.21.120.20, .appx)

Prerequisites: developer mode enabled (Settings → For developers).

- [ ] Pick an old version: the fe3 SOAP chain runs (GetConfig → GetCookie → SyncUpdates →
      GetExtendedUpdateInfo2); the download URL resolves and downloads; the SHA-256 from
      the Metadata database is verified.
- [ ] **[U]** The SOAP envelope bodies were written from the MS-WUSP shapes, not
      byte-compared with BedrockLauncher's StoreNetwork.cs — compare them once against
      the GPL-3 source (the parsing is lenient on namespaces; a drift must show up as a
      clear error, not a wrong download).
- [ ] The package extracts into `<instance>/game`, `AppxSignature.p7x` is deleted, the
      folder registers in developer mode at **launch** time (manifest at its final
      location), the game activates through the `minecraft:` protocol.
- [ ] Dependency failure fallback: on a machine missing the framework packages, the
      manifest is rewritten without `<PackageDependency>` entries (original kept as
      `AppxManifest-original.xml`) and the registration retried once.
- [ ] Data redirection for the legacy pipeline links the package family's LocalState
      (`%LOCALAPPDATA%\Packages\<family>\LocalState`); **[U]** verify the family name
      resolution (AUMID split, fallback `Name_PublisherId` from the full name).

## 4. Linux — experimental GDK-Proton backend

Prerequisites: Vulkan 1.3-class driver (RADV 26.1+ / NVIDIA 595+), internet, a Microsoft
account that owns the game, `tar` and `vulkaninfo` available.

- [ ] First launch of a GDK-era version: the setup task downloads, hash-verifies and
      unpacks, in order: the umu zipapp, the WineGDK engine archive, the GDK dependency
      DLLs, the CA bundle, the OpenSSL XCurl set, the Xodus client. All land under
      `<dataRoot>/bedrock/`. A mismatched hash aborts with "refused to install" and
      nothing unverified stays.
- [ ] **[U]** The SHA-256 pins (umu 1.4.3 zipapp + umu_run, engine archive, DLLs, OpenSSL
      set, Xodus p11 build) were re-verified when this step was written; re-verify the
      current upstream release still matches them, or update the pins deliberately
      (BedrockArtifacts.h).
- [ ] The licensing client needs a one-time login outside the launcher (its own HOME is
      `<dataRoot>/bedrock/xodus-home`): run `xodus-cli login` there once; the game data
      (`{xodus} run {game_dir}`) then extracts/decrypts into the instance's `game` folder.
- [ ] The per-instance prefix `proton-prefix` is created; the game runs through umu-run
      with PROTONPATH/WINEPREFIX set; the console shows the game's own output (LoggedProcess).
- [ ] **[U]** The umu environment as implemented sets PROTONPATH, WINEPREFIX,
      PROTON_VERB=run, GAMEID=0, STORE=none, UMU_FOLDERS_PATH and HOME (xodus-home).
      Compare once with BedrockOnLinux's `bol/launch.py`: the upstream recipe also sets
      Windows App Runtime bootstrap overrides (MICROSOFT_WINDOWSAPPRUNTIME_BOOTSTRAP_*)
      and DLL overrides whose exact values were not pinned in Phase 0 — add them here if
      the game fails to start.
- [ ] **[U]** WineGDK token registry value name ("RefreshToken" under
      `Software\Wine\WineGDK`): BedrockOnLinux documents the key, not the value name;
      verify against their code before relying on the in-game sign-in token.
- [ ] Vulkan check: on a machine with llvmpipe only, the launch must fail with the
      RADV/NVIDIA recommendation; a missing `vulkaninfo` is a warning, not a stop.
- [ ] Aborting a launch kills the game process; closing the console window while the
      game runs asks (same as Java behavior through the shared LaunchController path).
- [ ] Legacy UWP versions must fail pre-flight ("Windows-only") before anything downloads.
- [ ] Restart after everything is installed: no network is used for the engine/prefix
      (the game itself still needs internet per start — state as known limit).

## 5. Cross-cutting

- [ ] The version list still refreshes (GdkLinks/Metadata/Rayth; cached copies; snapshot
      fallback when offline) and the channel filter shows releases/previews/old.
- [ ] Deleting a Bedrock instance leaves `<dataRoot>/bedrock` (shared cache) alone; the
      junction in the game's data location is removed or re-pointed by the *next* launch
      of another instance (a leftover junction pointing at a deleted folder must be
      replaced, not followed).
- [ ] Settings page: separate-data checkbox (Windows), helper DLL path (Windows), Xodus
      path / custom launch command / MSA token (Linux) persist and round-trip.
- [ ] The game log page shows the launch log lines for every step (install, redirect,
      activation); the pre-flight lines describe the real pipeline being used.
