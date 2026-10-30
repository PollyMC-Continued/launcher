# Changelog

## v10.0.0

**This update brings the launcher's own new features together with a big core update.** New in the launcher itself: pinning instances, mod conflict detection, a smarter "Check instance" and the new "My Reports" page for your bug reports. Under the hood, the launcher's core has been brought up to date with Prism Launcher 11.1.1 (we were on 11.0.0 before), pulling in five upstream releases' worth of fixes and improvements. Offline accounts, offline skins and everything else you're used to work exactly as before, and your instances, accounts and settings carry over untouched.

**Added:**

- **Pin instances** — Right-click an instance (or use the instance toolbar) and pick "Pin Instance" to keep it at the top of its group, whatever the sort order is. Pinned instances show a small pin.
- **Mod conflict detection** — Two files with the same mod id, mods that declare themselves incompatible with each other (from the jar and from Modrinth/CurseForge data) and mods whose dependency is disabled or missing are now found. Conflicting mods get a red row in the mod list, and everything is listed in "Check instance".
- **Check instance: more checks** — Also looks at free disk space, whether the memory setting fits the computer, and mods not marked for the instance's game version. Results are sorted worst first, there is a "Check Again" button, and "Copy Report" puts the list on the clipboard.
- **Answers to your bug reports** — After you send a report you get a report ID (like K7X9F-3QD2M). When the team updates its status, the launcher shows a notification the next time it starts, e.g. "Fixed in version 9.4.3". Clicking it opens My Reports.
- **My Reports** — Lists the bug reports and suggestions you sent from this launcher, one card each: a colored status pill (Waiting, In review, Needs info, Fixed...), what you wrote, the team's answer in plain words, the type (Bug, Crash, Suggestion, Other), the date and the report ID with a "Copy ID" button. Open it from the "My Reports" button in the top corner of the report window (it counts reports with news), from "View My Reports" in the window that confirms your report was sent, or from the notification when the team answers. "Check for updates" looks again straight away.
- **Play offline when the login servers are down** — If Microsoft's login servers are unreachable, you now get a clear message with a retry button, and you can start the game in offline mode instead of being stuck at the login screen.
- **Warning for untrusted mods in imported modpacks** — When you import a Modrinth or CurseForge modpack containing mods that don't come from the platform itself, the launcher warns you before installing, so you know what you're getting.
- **Better graphics card detection** — On computers with more than one graphics card (like most gaming laptops), the launcher now asks the operating system directly which cards you have, and on Windows the fastest card is listed first. The game launches on the right card more often.
- **Clearer errors when the game won't start** — If Java refuses to start, the launcher now shows the actual reason in the log instead of a generic failure message.
- **Option to turn off the low memory warning** — If you deliberately run the game with little free memory, you can now switch the warning off in the settings.

**Changed:**

- **Check instance no longer freezes** — Mod jars are scanned in the background, so big modpacks don't lock the window.
- **Update checker is more honest** — The update window shows how many updates were found, and "up to date" no longer appears when some mods could not be checked.
- **Notifications don't overlap** — The GitHub star reminder now waits until the report check has finished, so the two never pop up at the same time.
- **Clearer thank-you message** — After sending a report, the window shows your report ID, a "View My Reports" button, and says the launcher will tell you when the team answers.
- **Memory slider now shows free memory** — In the Java settings, the slider shows how much memory is actually free right now instead of the total amount installed, making it much easier to pick a sensible value.
- **The launcher freezes less** — Settings are saved far less often and information is cached instead of being recalculated every time. The launcher feels snappier all round, especially on slower hard drives.
- **Faster instance list and mod lists** — Starting the launcher, browsing instances and opening the mods of a big modpack are all noticeably quicker.
- **More reliable CurseForge downloads** — Downloads from CurseForge's file servers are now more reliable.
- **Safer folder names for imported modpacks** — Special characters and symbols that cause trouble in folder names are kept out when the launcher creates an instance from a modpack.
- **The Modrinth modpack page now shows the pack's changelog** — You can see what's new in a pack before you install or update it.
- **Language settings are better behaved** — The confusing "Use system locale" option is now called "Use system regional standards", and turning it on or off no longer resets your chosen language by accident.
- **New Instance window order** — The install sources are now listed as Custom, Import, Modrinth, CurseForge, ATLauncher, FTB and then the rest.
- **A shorter More menu** — "My Reports" is no longer in the More menu. You can still open it from the report window, the report-sent window and the notification when the team answers.

**Fixed:**

- **Windows 11 theme** — selected rows in lists, trees and tables are now readable whatever their highlight color is: the text turns white on dark highlights and black on light ones (gray, green…). Unselected rows are unchanged.
- **Crash when changing the theme or icons in Settings** — The launcher could close right after picking a theme or icon set (seen on Windows 11). The window style is now swapped after the dropdown has finished closing, and it is no longer replaced when it is already the right one.
- **"Child launcher process failed" during the update check** — The launcher no longer shows this error when checking for updates. The check skips the splash screen, waits longer on slow computers, and falls back to the installed version if the check still can't finish.
- **Mod updates picking the wrong file** — When two updates had the same name, only one of them was downloaded (and the other was skipped or downloaded twice). Each update is now tracked on its own.
- **Update check on an empty mod folder** — Checking for updates with no mods installed could crash; it now says there is nothing to check.
- **Quilt mod dependencies** — They were read from the wrong place in quilt.mod.json and never found. Fabric's "java" requirement is no longer mistaken for a missing mod.
- **Native crash when launching certain modpacks** — A rare crash caused by background work being cleaned up at the wrong moment is fixed.
- **Background work is cleaned up safely** — When a window or list closes, its leftover background work is now stopped properly, preventing rare crashes.
- **Safer modpack imports (security)** — A maliciously crafted modpack could, in certain cases, place files outside the instance folder during import. This affected CurseForge and ATLauncher packs and is now blocked.
- **Importing the same modpack twice no longer causes conflicts** — Each import now gets its own identity instead of sharing one with the original.
- **Renaming an instance while the game is running is no longer allowed** — This used to break the running instance.
- **Instance shortcuts** — Shortcuts keep working when you rename an instance, and existing shortcuts are found again after restarting the launcher.
- **Duplicating an instance** — The copy now gets its own identity, so the two no longer clash.
- **Crash when installing a mod loader that was disabled** — Fixed.
- **Mod dependencies of dependencies** — Enabling or disabling a mod now correctly handles mods that sit behind other dependencies as well.
- **Microsoft accounts are kept until sign-in works** — The launcher no longer removes an account just because a re-login attempt failed; your account stays in the list.
- **No more empty account picker** — The launcher no longer opens the account selection window when there are no accounts to choose from.
- **Offline accounts are refreshed properly at launch** — Skins and profiles update as they should when you start the game.
- **A rare case where the launcher's own folders could be deleted during cleanup** — Fixed. Your data is safe.
- **Crash when opening the "Other logs" page** — Fixed.
- **Crash when closing the data packs window** — Fixed, along with a small memory leak.
- **Sign-in with a code now shows the real error** — Instead of failing silently, you see what actually went wrong.
- **Updating an installed modpack works again** — The "Update" button on an installed Modrinth or CurseForge pack no longer fails.
- **macOS fixes** — The menu bar is readable again in dark mode, and Cmd+Q now quits the whole launcher instead of closing only the active window.
- **Translation downloads** — If the list of translations can't be fetched, your chosen language no longer falls back to English.
- **Crash when opening or importing some instances (0xC0000005)** — Instances whose packwiz metadata contains unusual Minecraft versions could crash the launcher on a background thread while the mod list was loading.
- **Version sorting** — Minecraft version strings are now compared with a total ordering, so sorting mod metadata can no longer walk past the end of the list and read garbage memory.

**Where to find the mod incompatibility option:**

- Mod conflicts (duplicate mods, mods that conflict with each other, disabled or missing dependencies) are shown in the mod list and in "Check instance".
- The option **Detect and show mod incompatibilities (experimental)** is off by default. It controls the highlighting of mods that are not marked as compatible with the instance's Minecraft version. Turn it on in **Settings > General > Mods and Modpacks**.

**Notes:**

- Your instances, accounts, settings and skins carry over exactly as they are — nothing to do after updating.
- Offline accounts, offline skins and the launcher's other own features work exactly as before.

## v9.5.1

**Changed:**

- **Clearer report sending** — The bug report window now shows a progress bar and tells you what it's doing while your report is sent.
- **Clearer report errors** — If a report can't be sent, the message now tells you what to do.

**Fixed:**

- **Spam reports blocked** — Bug reports are now protected against spam, so the team's inbox stays usable.

**Note:** Older launcher versions can no longer send bug reports. Please update to keep reporting problems.

## v9.5.0

**Added:**

- **In-launcher bug reports** — Help > Report a Bug lets you send a bug, crash or suggestion to the team, with optional screenshots and the launcher log (personal paths hidden).
- **Crash recovery prompt** — If the launcher closed without warning last time, it offers to send a crash report with the previous session's log attached.
- **Copy diagnostics** — Help > Copy Diagnostics puts the launcher version, system, Qt and Java details on your clipboard in one click, ready to paste into a support chat.
- **Instance health check** — Right-click an instance (or use the instance toolbar) and pick "Check instance" to find a missing Java, a broken mod jar or a wrong loader version before you launch.
- **Search in settings** — The settings window has a search box (Ctrl+F) that jumps straight to a setting. It understands several words, tooltips and drop-down choices, forgives skipped letters, shows where each result lives and opens the right tab for you.
- **Language from your region** — On first run, if your system language has no translation, the launcher picks one from your region (via a quick country lookup) and says so on the language page. Choosing a language yourself always wins.
- **Mod download repair** — When a download fails with "Permission denied", the error window now offers to retry through a different temp folder or to open the antivirus exclusion guide.

**Changed:**

- **New splash screen** — The startup splash has a fresh look (logo on a soft glow, an animated progress bar and a status line) and shows what the launcher is doing. The status text uses the language you picked, as far as it is translated. It also no longer stays over other windows such as Settings: it closes as soon as any other window opens.
- **Faster first-run translations** — The window now appears right away and switches to your language as soon as it finishes downloading, instead of holding back the UI.
- **Quieter update checks** — A failed update check no longer disturbs startup: it just leaves a short note in the status bar, and a failed check can never take the launcher down with it.
- **Remembered window sizes** — More windows (About, version pickers, export and copy dialogs, the network error window and others) now reopen at the size you left them.
- **Windows Qt pinned to 6.11.2** — Qt 6.12.0 is unstable and breaks on some devices, so the Windows build now stays on 6.11.2 instead of the newest version MSYS2 ships. The build fails if the pin does not take effect.


**Fixed:**

- **Report drafts are kept** — Closing the "Report a Bug" window by accident no longer throws away what you typed; it is restored the next time you open it.
- **Duplicate report warning** — If you already sent the same report this session, the launcher says so before sending it again.
- **Download errors** — Failed downloads now show the real reason (such as "Permission denied"), are retried after a short pause, and are written to the log. Task failures are no longer hidden from the log by default.

## v9.4.1

**Added:**

- **Automatic translation source sync** — A new `update-translations.yml` workflow runs `lupdate` whenever a `.cpp`, `.h` or `.ui` file under `launcher/` changes on `main` (or when started by hand from the Actions tab), so `translations/*.ts` always matches the current source strings. It commits only when a text was added, changed or removed — runs where only line numbers moved are discarded, so an ordinary code change no longer produces a commit touching every `.ts` file. After a real change it starts `gh-pages.yml` with `scope=translations` to rebuild the `.qm` files and redeploy the site, since pushes made with the default workflow token do not trigger other workflows. The workflow does not watch `translations/`, so its own commit cannot re-trigger it.

**Changed:**

- **Updater renamed to PollyMC** — The updater's settings file, update log, marker files, temp folder, classes and source files now use PollyMC names instead of Prism names. Existing settings, logs and update markers from older releases are still read, so skipped versions are kept and updating from an older release still reports its result.

**Fixed:**

- **Star reminder toast bugs** — The "Star on GitHub" card now shows its close button correctly, sits above the status bar even with longer translated text, follows theme and language changes while open, stays on top of other widgets, repositions correctly when the status bar is toggled, and pauses auto-hide while the mouse is over it. It also scales with the UI font and screen scaling, wraps long titles instead of cutting them off, uses a plain ★ instead of an emoji some systems can't draw, and shows "Star on GitHub" as a proper button.
- **Build workflow fixes** — Fixed macOS packaging failing with "Permission denied" on `verify_bundle.sh`, a cross-distro test that could never fail, shell injection through branch and tag names, invalid versions only failing late, and release gating that needed manual edits for every new job; downloads now retry, the Windows artifact upload fails when files are missing, and the changelog check no longer re-scans commits.

## v9.4.0

**Added:**

- **Portable data migration** — Portable installs now offer to migrate data from an existing system install on first launch.
- **Launcher font picker** — Settings > Appearance now has a Launcher Font dropdown (same font list as the console font) with live preview across the whole launcher.
- **Command palette** — Press Ctrl+Shift+P anywhere in the main window to fuzzy-search every launcher action and run it from the keyboard.
- **Bot scripting** — Attach say/wait/loop scripts to bots from the new Script tab in the Bot Manager, with a built-in step editor and per-bot run control.
- **Natural-language command search** — Prefix a palette query with `?` to match commands by meaning instead of name, using a downloadable multilingual model that runs fully offline after a one-time download.
- **Startup splash screen** — A splash appears immediately on launch, before
  heavy initialization, so slow systems and Winlator users get visual feedback
  instead of a blank delay.
- **Forced auto-update with opt-out** — New "Update behavior" dropdown in
  Settings > Launcher: install updates silently (default), ask first, or
  never check. Silent updates defer automatically if a game is running.
- **Persistent model consent** — The first-run download prompt for the
  natural-language model is now remembered. If you accepted once, future
  model updates download automatically without prompting.
- **Discord Rich Presence toggle** — Settings > General has a
  new "Show what you're playing on Discord" checkbox. Discord
  activity was always shared before; it's on by default, and
  turning it off now applies immediately without a restart.
- **Separate dark/light and widget style** — Settings >
  Appearance now has an "Appearance" dropdown (Light / Dark /
  System) independent of the "Theme" dropdown (which picks
  the widget style). Previously dark and light were baked
  into each theme; now they apply on top of any non-native
  theme. Windows-native themes continue to follow your
  Windows appearance setting and grey out the Appearance
  dropdown.

**Fixed:** 

- **Windows 11 theme now renders correctly on Windows 10** — The style draws spin arrows, checkboxes, and
  other indicators with glyphs from Segoe Fluent Icons, which
  isn't present on Windows 10 or Wine. Those
  primitives now fall back to Fusion's rendering while the rest
  of the Windows 11 style is unchanged.
- **Switching away from a Windows theme and back no longer
  corrupts its look** — Re-selecting a native Windows style
  used to stamp a default palette over the native rendering,
  producing mismatched colors. Native styles are now left
  untouched on every apply.
- **OAuth response bodies are no longer logged** — A failed
  Microsoft login used to write the full token-endpoint response
  into the launcher log, which could expose the access token to
  anyone the log was shared with. Only the HTTP status is now
  recorded.
- **Bots no longer stay online after quit** — The bot server now disconnects all bots and exits when the launcher closes, with a parent-process watchdog as backup.
- **Bot respawn works** — Uses the vanilla respawn packet instead of a nonexistent API call, and skips when the bot isn't dead.
- **Bot movement right after join** — Pathfinding is set up at login instead of spawn, and bad coordinates or a missing item count no longer throw.
- **Clearer bot errors** — Input parse failures and command failures are now reported separately instead of everything showing as invalid JSON.
- **AppImage runs on older Linux distributions** — The AppImage was built on Ubuntu 24.04, so everything bundled into it (the launcher, `AppRun`, Qt, libarchive, ONNX Runtime) needed glibc 2.38 or newer and failed to start on Ubuntu 22.04, Debian 12 and similar systems with errors like ``version `GLIBC_2.38' not found``. It is now built against glibc 2.35 (Qt 6.8.3 from the official Qt installer, everything else from Ubuntu 22.04), and CI both asserts the glibc floor and launches the AppImage inside an Ubuntu 22.04 container so this cannot regress silently.
- **AppImage file name** — It is now `PollyMC-Continued-<version>-x86_64.AppImage` instead of `...-Linux-x86_64.AppImage`; the AppImage packaging guidelines explicitly ask not to put "Linux" in an AppImage name since every AppImage is for Linux. The name is now applied when the image is created, not patched afterwards.
- **Translations no longer delay the first paint** — The translation index download now starts only after the event loop is running, so the launcher paints immediately in English (or a cached translation) and applies the downloaded language when it arrives.
- **Update check no longer freezes or crashes startup** — The update check used to run synchronously on the UI thread, blocking it for up to 60 seconds, and fired immediately on a first launch (and from inside the updater's constructor with the "On Launch" setting). It now runs in the background, waits a few seconds after startup before the first automatic check, and fails silently when automatic (manual checks still report errors).

**Changed:**

- **AppImage widget style** — The AppImage no longer bundles the KDE Breeze widget style, because no distro package provides a build matching the Qt used for the AppImage. Breeze icons are unaffected: they are compiled into the launcher itself. The `.deb`, tarball and Arch packages keep it.
- **Translations moved to pollymc.vercel.app** — Previously hosted on
  GitHub Pages. The old URL is still served for backward compatibility
  with older launcher builds.
- **Clearer error dialogs** — Launcher error messages now lead
  with what went wrong and put the technical details behind a
  "Show Details" expander, instead of dumping paths and version
  numbers into the main body.
- **Translation strings updated** — Several launcher dialogs and
  status messages have new source strings. Existing translations
  will fall back to English until translators catch up.
- **"Use discrete GPU" now works on Windows** — The toggle
  previously did nothing on Windows, since the GPU preference
  logic was Linux-only. Enabling it now writes the same
  per-application GPU preference that Windows Settings >
  Display > Graphics uses, for the Java binary the instance
  actually launches. Note: this is per-binary — it affects
  every app on the machine using the same javaw.exe.
  
## v9.3.0

**Added:**

- **Online skin browser** — Browse and search skins from crafty.gg directly in the skin manager.
- **Use skins instantly** — Pick a skin and it gets downloaded, added to your library, and applied automatically.
- **Better translations** — Hindi, Bengali, French are now fully translated, with several broken translation strings fixed.
- **Skin browsing improvements** — Online skins are cached, so previously viewed textures don't need to be downloaded again.
- **Discord Rich Presence** — Shows when you're browsing skins and gives more accurate launcher and game status.

**Changed:**

- **Faster startup** — Reduced unnecessary work while the launcher is opening.
- **Faster instance list** — Large instance folders now sort and load more efficiently.
- **Better caching** — Icon and screenshot cache is now limited to 128 MB.
- **Cleaner window titles** — Window titles now use a simpler launcher and version format.
- **Improved toolbar** — Related options are now grouped into clearer menus.
- **Better tooltips** — More launcher menus and settings now have helpful tooltips.
- **Improved packaging** — Linux, Windows and `.deb` builds now handle version and dependency details more reliably.

**Fixed:**

- **Instances no longer disappear after updates** — The launcher can now detect certain data-folder changes and help recover stranded instances.
- **Portable installs work more reliably** — Portable mode now correctly finds its marker regardless of where the launcher is started from.
- **Windows upgrades are safer** — Updates no longer risk removing important portable or instance data.
- **GitHub star reminder** — Fixed animation and resizing issues with the star reminder.
- **Linux compatibility** — Bundled libraries now prevent launch failures on some distributions.
- **World imports** — World ZIP files with `level.dat` at the root are recognized correctly again.
- **Skin applying** — Expired launcher tokens are refreshed before uploading a skin.
- **Discord Rich Presence** — Fixed several cases where launcher and game status could be missing, outdated or incorrect.
- **Skin search** — Searching by player, tag or style now returns the expected results.
- **Translation fixes** — Fixed numerous broken placeholders, formatting issues and missing translated text.

**Removed:**

- **Unused files** — Removed old installer and release files that were no longer needed.

## v9.2.7

**Added:** 

- Linux binary tarball release artifact (`PollyMC-Continued-*-Linux-x86_64.tar.gz`) — portable install with the same layout as the Windows portable zip
- Finnish, Hungarian, Japanese, and Laotian translations completed to 100%

**Changed:**

- Release `.deb` and `.pkg.tar.zst` artifacts renamed to the `PollyMC-Continued-*` naming scheme (package install names unchanged)
- Mods page table columns now resize proportionally with the window — Name, Last Modified, and Provider stretch; the Enable checkbox column sizes to its content
- Dropped the unused pacman database files (`pollymc-continued.db`/`.files`) from build artifacts
- Mod folder parsing (zip handling, metadata, icon decoding) now runs off the UI thread, so loading large mod folders no longer freezes the launcher
- Column-layout saves and the resource browser search are now debounced instead of firing on every pixel of a drag or every keystroke
- Bot system no longer bundles Node.js modules into installs — they are downloaded on first use
- Bot Manager now checks for Node.js and the required modules when opened, prompting the user to install them when missing

**Fixed:**

- Launcher ignored display scaling at 125% and 150% on Windows and macOS (the rounding policy floored fractional scales to 100%); the policy now only applies on Linux
- Closing the Java wizard after enabling automatic Java download crashed the launcher (double-free of a layout item)
- Jar mods were leaked on every launch (`getJarMods()` returned unowned objects)
- Setup wizard, category filter checkboxes, and in-flight icon downloads leaked when dismissed or closed
- Mod dependency relationships rebuilt in O(n²) time on large mod folders — now uses lookup tables
- Resolving the cache could stall the UI with a full-file MD5 read and unbounded `xdg-mime` wait
- Bot dependency install could hang forever if npm failed to start; the bot status now shows the install progress
- Launcher shutdown no longer blocks on draining the global thread pool

## v9.2.6

**Added:**

- Debian/Ubuntu apt repository — `.deb` packages installable via `sudo apt install pollymc-continued`, hosted on GitHub Pages (optionally GPG-signed)
- Arch Linux pacman repository — `.pkg.tar.zst` packages installable via `sudo pacman -S pollymc-continued`, hosted on GitHub Pages
- `deploy-repos.yml` workflow publishing both repositories to GitHub Pages on every release
- README "Install" section covering both package managers

**Changed:**

- Releases now include `.deb` and `.pkg.tar.zst` packages
- Account page button now reads "Add &amp;Authlib-injector"
- Translations now load from the `translations` subdirectory of our GitHub Pages site

**Fixed:**

- Afrikaans translation now 100% complete

**Removed:**

- Joke languages (`en@pirate`, `en@uwu`, `grc`, `jam`, `peo`, `rue`) and their translation files

## v9.2.5

**Fixed:**

- Bundle Qt image format plugins in Linux AppImage
- Repair malformed plural entries in all translations so every language compiles again
- Custom Skin not loading on singleplayer world
- Cache skins resolved

## v9.2.4

**Added:**

- Linux man page (`pollymc.6`) for the launcher
- Installer now asks before creating a desktop shortcut
- Silent installs skip the desktop shortcut prompt automatically and don't touch the desktop
- Translation pipeline restored: `.ts` files are compiled to `.qm` and published to our own GitHub Pages
- Translations now load from our GitHub Pages instead of Prism's i18n server
- "Help us with translations!" link now points to our repository
- CONTRIBUTING.md with the translation contribution flow (fork, edit `.ts`, open a PR)

**Fixed:**

- NeoForge 1.21.x (and modern Forge) instances no longer crash on launch when using an offline account: the skin agent no longer claims the one-time slot — it now registers a protocol handler leaving the slot free for NeoForge/Quilt
- Auto-updater now detects new releases published under the same version number
- Installer and uninstaller now use the PollyMC icon instead of the NSIS default

## v9.2.3

**Added:**

- More bugs to fix later. You're welcome. (It's this changelog entry, but let's be honest: it reads as a feature at this point)

**Fixed:**

- Language selection now lists all available translations instead of only English
- "Help us with translations!" link now points to [NEEDS UPDATE — Weblate reference is stale, tell me the correct current target]
- Minecraft 1.8.9 and other Java 8 instances no longer fail to start when using an offline account: the skin agent's module options, which only Java 9+ understands, are no longer passed to Java 8
- Updates are now only offered for actual installable files: checksum and signature files attached to releases (e.g. `.sha256`, `.asc`) can no longer be mistaken for an update
- Auto-updater no longer leaves a full copy of the update inside the install folder: extraction now happens in the system temp folder, and extraction leftovers from older updates are removed when the update installs
- Portable installs now reliably receive files that are new to a release: the updater installs the file list shipped with the update instead of guessing from the old install folder
- Help buttons that previously opened a dead link (and did nothing at all) now say what's what instead of silently failing — pending documentation, they politely tell you the page is "coming later"

**Removed:**

- Cat feature: "Meow" button (toolbar and View menu), cat packs folder entry, cat background/overlay rendering, cat pack selector and cat scaling/opacity settings in Appearance, the CatPack/CatPainter classes and all cat icon assets, and "And cat :3" from the Linux AppStream metadata

## v9.2.2

**Changed:**

- Linux AppImage bundles the KDE Breeze widget style
- Fractional screen scaling rounds sanely under KDE Plasma; per-screen scale factors logged at startup
- Java diagnostics redact secret-looking environment values (TOKEN/KEY/SECRET/PASSWORD/AUTH)

**Fixed:**

- Portable Windows zip now includes `portable.txt`, so the launcher and auto-updater detect and update portable installs correctly
- Windows installer asset renamed to include "Windows" (e.g. `PollyMC-Continued-9.2.2-Windows-Setup.exe`) so the auto-updater can match it

## v9.2.1

**Changed:**

- Bot Manager: commands that act on a bot no longer take a username — you must select the bot(s) in the table first; multi-selected bots are targeted together, and `/quit` now disconnects the selected bot(s) instead of `<username>`

**Fixed:**

- Quilt instances no longer fail to launch when using an offline account
- "Not enough RAM" warning now compares the allocation against total installed RAM instead of momentary free RAM
- Updater no longer reports a new version when the installed version matches the latest release (macOS builds now carry a build number distinct from the marketing version)

## v9.2.0

**Added:**

- Bot system: manage Minecraft bots in the new Bot Manager window (Help menu) — add, edit, remove, start and stop bots, multi-select rows
- Bot Manager: console commands `/join <server> [username] [port]`, `/quit`, `/list`, `/say <message>`, `/follow <player>`, `/stop`, `/goto <x> <y> <z>`, `/home`, `/pos`, `/health`, `/inventory`, `/drop <item> [count]`, `/equip <item>`, `/whisper <player> <message>`, `/respawn`, `/players`, `/help`, `/clear` (clears the console)
- Bot Manager: Commands button showing every command with its usage
- Bot Manager: slash-command autocomplete popup — type `/` in the console to see available commands
- Bot Manager: per-bot Minecraft version picker, actually used when the bot connects
- System tray icon with Show/Quit menu — "Minimize to Tray" no longer makes the app unreachable

**Changed:**

- Bot Manager: Send button now inherits the launcher theme instead of a hardcoded green

**Removed:**

- Bot Manager: "Microsoft account" login option (not implemented)
- Dead BotTerminalPage code

**Fixed:**

- Auto-updater: release list fetch blocked by a malformed repo URL
- Updater layout: restore checkboxes no longer overlap the interval spinner
- Discord Rich Presence: IPC probing no longer freezes the UI
- Plaintext account password no longer written to `accounts.json` on hard auth failure
- Auth server URL validated in login dialog (auto-prepends `https://`, rejects non-http schemes)
- Yggdrasil skin model read from texture metadata ("slim"/"classic") instead of hardcoded slim
- Multi-profile picker now has a proper parent window
- Bot Manager: opening it twice no longer spawns duplicate bot servers
- Bot Manager: Stop actually disconnects bots
- Bot Manager: Add/Edit dialog can't accept an empty bot name (cancelled vs empty now unambiguous)
- Bot Manager: `/join` supports a port argument
- Bot Manager: duplicate bot names rejected

## v9.1.1

**Added:**

- Detect auth server downtime and launch offline with clearer dialog ("Auth servers offline" vs "No internet connection")
- Retain network error info across auth steps for better offline fallback messaging
- Skip pointless retries on HTTP 404 responses in NetJob

**Changed:**

- Update README: remove downloads section, add contributor avatars
- Add Discord badge and enable Discord Guild menu item in launcher
- Installer now uses CI version instead of hardcoded "1.0.0" for registry DisplayVersion
- Enable auto-updater (Windows/Linux via GitHub releases, macOS via Sparkle)

**Fixed:**

- Memory leak and potential crash in data packs modal dialog (non-modal with WA_DeleteOnClose)
- Infinite update check loop when timer fires during an ongoing check (re-entrancy guard)
- Updater now builds and runs on all platforms (removed `NOT APPLE` guard); macOS falls back to PrismExternalUpdater when Sparkle feed URL is empty

## v9.1.0

**Added:**

- Yggdrasil (authlib-injector) account support
  - New AccountType::AuthlibInjector
  - AuthlibInjectorStep for /authserver/authenticate and /authserver/refresh
  - Login dialog with server URL, username, password
  - JVM agent injection for authlib-injector at launch
  - "Add Yggdrasil" toolbar button
  - Skin fetching via sessionserver profile endpoint
  - Multi-profile picker when server returns multiple availableProfiles
- macOS derives version from git tag (like Windows/Linux)

**Changed:**

- Add ccache to CI builds (Windows, Linux, macOS) for faster rebuilds

**Removed:**

- Unused accountIsOnline variable

**Fixed:**

- Cancel button not working in Yggdrasil login dialog
- Yggdrasil accounts not fetching player skins
- Old PollyMC accounts with missing auth server URL now show clear error
- PollyMC ↔ PollyMC-Continued account compatibility (reads both `authlibInjectorUrl` and `auth-server-url`, writes `authlibInjectorUrl`)
- Use accountData() accessor instead of protected data member
- authlib-injector.jar not found on AppImage (`applicationDirPath()` resolves to temp mount; added `dataRoot()/libraries/` as persistent fallback)

## v9.0.9

**Added:**

- Portable macOS build and DMG
- Reliable local macOS build script
- Local macOS build setup hardening
- qtwebsockets module to build workflow
- Qt 6.11.* support (bump from 6.9.3)
- macOS deployment target bumped to 13.0
- Contributors section in README
- Branch name check skips main/master/develop

**Changed:**

- Revise README for clarity and feature highlights
- Shorten README

**Fixed:**

- Use --codesigning=off for Qt 6.9+ macdeployqt
- Remove codesign flags for Qt 6.9+ macdeployqt
- verify_bundle.sh for Qt 6.11: handle @rpath/Frameworks and bundle-internal framework deps
- Skip binary files in branch name grep to avoid matching 'main' in compiled code

## v9.0.8

**Added:**

- Dll Checks in Github Action
- Enhance build script with DLL checks and updates
- Enhance release workflow with additional triggers
- Enhance versioning logic in build.yml

**Changed:**

- Refactor CI workflow for Windows and Linux builds
- Refactor DLL dependency handling in build workflow
- Refactor CMake build process and improve logging
- Refactor deployment script for improved clarity
- Update build permissions and fix version handling
- Update release workflow to handle versioning
- Improve DLL deployment logic

**Fixed:**

- Filter Windows system DLLs from recursive dep check; add installer DLL test step
- Add msvcp_win.dll to Windows system DLL skip list
- Replace libgamemode-dev with gamemode-dev for Ubuntu 24.04
- Cache linuxdeploy step with proper key and restore-keys
- Formatting and error messages in build.yml
- Formatting issue in build.yml
- GitHub actions workflow bugs (×2)
- Icon naming in .desktop file

## v9.0.7

**Added:**

- Bundle OpenSSL into AppImage

**Fixed:**

- NSIS script and DLL copy locations

## v9.0.6

**Fixed:**

- Reduce parallel jobs to -j2 for Windows build

## v9.0.5

**Fixed:**

- NSIS installer output directory bug

## v9.0.4

**Fixed:**

- Limit parallel jobs in build step

## v9.0.3

**Fixed:**

- Add error checking to Windows build steps

## v9.0.2

**Other:**

- Debug: Add more file verification steps

## v9.0.1

**Changed:**

- Update CI workflow
- Update pollymc icon, remove old files

**Fixed:**

- Deploy MinGW runtime DLLs in Windows build
- Resolve release conflict in CI workflow
- Let GitHub auto-generate release notes from commits

**Other:**

- Debug: Verify release files exist before upload
