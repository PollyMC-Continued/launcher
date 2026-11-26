# Changelog

## v10.2.0

**Bedrock Edition support: a new "Bedrock" instance type beside the Java instances, creatable from the New Instance dialog's Minecraft Edition switch with a live version list — and, behind one pipeline, the real backends: the Windows GDK install with SHA-256-verified packages and the entitled install through Windows, the legacy UWP extract-and-register path for old versions, per-instance game data, and the experimental Linux GDK-Proton backend.**

**Added:**

- **Bedrock Edition instances (groundwork)** — The launcher now understands a second instance type, "Bedrock", stored in `instance.cfg` next to the familiar "OneSix". A Bedrock instance is a data profile — its folder holds the chosen game version and, later, worlds, resource packs and behavior packs — and it deliberately shares nothing with the Java machinery: no PackProfile components, no Java checks, no Mojang account login. Creating, copying, renaming, grouping, notes, playtime tracking and the rest of the launcher's instance management all work on it unchanged. Its window opens with a minimal set of pages (game log, version, settings, notes and other logs; resource packs, behavior packs and worlds arrive with content support). Launching is wired through the same launch path as Java with the Java-specific parts — account selection and refresh, online/offline/demo modes, server status and profilers — skipped. Java instances are launched exactly as before.
- **The Bedrock version list** — The Version page of a Bedrock instance now offers the real thing. It merges the community version databases the design was verified against — GdkLinks for the current GDK package links, Metadata for the release history back to 0.12.0.1 with file names and SHA-256 hashes, and the Rayth Network list as a mirror-only supplement — fetched through the launcher's normal download stack and cached on disk. A failed refresh keeps the previously loaded list rather than emptying it, and a small snapshot shipped inside the launcher is the last resort, so the list never goes silently blank. A channel filter offers all versions, releases, previews and the old legacy-UWP versions; the dead Beta channel never appears. Each version is classified by what its actual package is — plain .Appx versus encrypted .msixvc — instead of by date arithmetic, so versions around the 2025 packaging switch land on the right side of the divide. Selecting a version stores it on the instance (the statusbar shows it from then on), and hovering a row shows what the databases know about it: file name, hash, download links, update ids.
- **Bedrock launch attempts say what is happening** — Pressing Launch on a Bedrock instance used to open the console window and show nothing at all. The launch now runs a pre-flight check through the same console path as a Java launch: it prints the selected version with its channel and packaging and the exact pipeline that will be used for it — or, if no version is selected yet, it says so and points at the Version page. Nothing fails silently.
- **The Windows backend: install and launch behind one pipeline** — One install task serves both Windows packaging generations, picked by what the chosen version actually is. Current versions (GDK era): the .msixvc package is downloaded from the Xbox CDN links the version databases carry, with mirror rotation and SHA-256 verification against the databases' hashes into a shared package cache, and then handed to Windows itself through PackageManager.AddPackageAsync — the entitled install, which needs a Microsoft account that owns the game; the launcher never decrypts anything. Legacy versions (up to 1.21.120.20): the Microsoft delivery service (fe3) resolves the download when the databases carry no direct URL, the .appx is hash-verified and extracted into the instance, and the signature-stripped folder registers in developer mode at launch time. The launch itself activates the installed game through its AppUserModelID (GDK) or its minecraft: protocol (legacy), and Windows' own deployment errors are surfaced verbatim instead of being swallowed.
- **One shared game, per-instance data** — On Windows the installed game is shared between Bedrock instances (only one GDK-era version at a time): switching an instance to a different GDK version warns first and reinstalls the shared game. What is per-instance is the data: the game's data folder (`%APPDATA%\Minecraft Bedrock` for the GDK era, the package's LocalState for legacy) is linked to the instance's own folder on launch, so each instance keeps its own worlds, packs and settings — the launched instance's data is the active one, the first redirect asks before adopting the game's existing data, and a folder the game recreates after a reinstall is kept as a backup instead of being destroyed. A Settings checkbox opts out (shared data), and the optional BedrockLauncher helper DLL that pins the exact installed version can be configured per instance.
- **The Linux backend (experimental)** — Bedrock on Linux runs the Windows game through the GDK-Proton engine inside umu-launcher, with the external Xodus licensing client signing in with the user's own Microsoft account to download and decrypt the game — nothing of it is bundled with the launcher: the engine, umu, the open-source GDK dependency DLLs and the licensing client are all fetched on first use, verified against pinned SHA-256 values (a mismatch is refused, not used), and cached. Each instance gets its own Proton prefix, the launch checks for a Vulkan 1.3-class driver (the D3D12 renderer needs it — RADV 26.1+ or NVIDIA 595+ recommended), and the game's own output shows in the launcher's console. Known limits, stated in the UI: achievements do not unlock, the game needs internet at every start, and legacy UWP versions are Windows-only. An instance's Settings page can point at a locally built licensing client, customize the launch command, or supply a Microsoft account token for the in-game sign-in.
- **Creating Bedrock instances from the UI** — The New Instance dialog has a Minecraft Edition selector — Java Edition or Bedrock Edition. Java keeps the familiar pages exactly as they were (version and mod loaders, import from file or URL, Modrinth, CurseForge, ATLauncher, FTB, Technic); Bedrock swaps the whole area for its own page: the same Bedrock version list the instance Version page uses, with the channel filter (releases by default), a search box and Refresh. The recommended release comes preselected, so picking a different version and pressing OK is all it takes; the name suggestion follows the selected version, and switching the edition back and forth keeps both sides' choices. The created instance carries the chosen game version on its Version page, shows it in the launcher's statusbar, and can download the game files right away or on first launch. There are deliberately no mod loaders and no modpack providers to browse for Bedrock in this phase.

**Notes:**

- To try Bedrock support, open **Add Instance**, pick **Bedrock Edition** under Minecraft Edition, choose a version and press OK — no more hand-written `instance.cfg`. (An existing folder whose `instance.cfg` contains `InstanceType=Bedrock` still works too.) The version list needs internet the first time it opens; afterwards a cached copy is used and Refresh fetches it fresh. On Windows the install runs on first launch (or right on creation when the box is ticked); on Linux the first launch downloads the experimental backend's engine, licensing client and helpers before the game runs. A manual test checklist for both platforms ships with the source (`launcher/bedrock/TESTING.md`).

## v10.1.2

**Gets rid of the "Launcher closed unexpectedly" window, helps PCs with old graphics run Minecraft, makes offline skins work on old Minecraft versions, and gives your bug reports five new answers.**

**Fixed:**

- **No more "Launcher closed unexpectedly" window** — After the launcher had closed without warning, the next start opened a window offering to send a crash report. That window is gone for good, along with its checkbox in the launcher settings, so a crash no longer greets you with a dialog at the next launch. You can still send a report whenever you like with **Report a Bug**.
- **Offline skins on old Minecraft versions** — With **legacy online tweaks** turned on, an offline account in an old Minecraft version got the skin of the online player with the same username instead of the skin you picked in the skin manager, because old versions ask for a skin by player name and the launcher looked that name up online. The launcher now gives the game your account's own skin first, with the right arm width, and your offline cape too. Other players' skins still come from the online lookup, and an account with no skin picked still falls back to it as before.

**Added:**

- **Software rendering for PCs with old graphics (Windows)** — If Minecraft crashes right at startup because your graphics card or driver is too old or missing, you can now turn on **Use software rendering** in an instance's settings (Performance). The game then draws with your processor instead, which works on almost any PC but is slow. The renderer is downloaded once on the first launch.
- **Graphics driver guide (Windows)** — A step by step guide for finding and installing the right Intel, AMD or NVIDIA driver for your graphics card, with direct links, from the same settings page.
- **Graphics hardware warning (Windows)** — When the launcher detects graphics hardware that is probably too old for Minecraft, it now tells you before launching and offers the guide or software rendering instead of letting the game crash.
- **Five new answers for your bug reports** — **My Reports** can now tell you more precisely what happened to a report: *A fix is being worked on*, *The team couldn't reproduce it - more details would help*, and *Not caused by the launcher* (most likely a mod, Java or the game). Suggestions can now also be marked *Planned* and *Added*, instead of only "Fixed". Launchers older than this version show a new answer as a plain *Updated*.

## v10.1.1

**Fixes for updating on Windows: the installer no longer deletes files before you confirm, it waits for locked files to be released, and the update prompts no longer abort or skip files by mistake.**

**Fixed:**

- **"Error opening file for writing" when updating on Windows** — The installer used to delete the old program files as soon as it opened, and it only closed `pollymc.exe`. The updater and filelink processes could still hold files, so the install failed partway. The installer now stops all three processes and only removes the old files after you confirm the upgrade. If a file is still locked, it retries for about 15 seconds before giving up.
- **Cancelling an update no longer removes the installed launcher** — Cancelling the upgrade question at the start of the installer leaves the existing install untouched.
- **Ignore no longer leaves a mixed install** — A locked file in the installer now offers Retry or Cancel instead of Ignore, so an update can't finish with some old and some new files.
- **"Update interrupted" lock dialog** — Choosing **Ignore** now clears the stale lock file and continues the update. Before, it aborted, so the lock file was never removed and the same prompt came back on every attempt.

## v10.1.0

**Six new features in one update: joining a server the moment the game starts, a much smarter bot manager (variables, conditions, auto-reconnect and a live map), offline capes from crafty.gg with a matching elytra, playtime and performance statistics for every instance, reusable instance templates, and updating every instance's mods in one go.**

**Added:**

- **Join a server the moment the game starts** — The Servers page of an instance has a new **Join on launch** button next to **Join**. Turn it on for a server and the next launch of that instance skips the title screen and the server list and connects straight to it; the server's row is marked "[auto-join]" so you can see which one it is at a glance, and turning it off again forgets the server. It feeds the launcher's quick-play behaviour, which already knew how to connect: modern game versions use the quick-play argument, older ones the classic `--server` and `--port`, and launching into a singleplayer world still needs Minecraft 1.20 or newer.
- **Bots that look after themselves** — When you add or edit a bot there are four new options. **Reconnect automatically** brings a bot back after a connection drop: it waits 5 seconds before the first retry, then 10, 20 and 40 seconds, and never longer than a minute, with unlimited retries that start over once the bot is back in. **Respawn automatically after death** presses the respawn button on its own. **Anti-AFK look interval** makes the bot glance around and now and then swing an arm, every few seconds or minutes as you choose, so it never stands perfectly still — it only turns its head and never walks, so scripts and pathfinding are untouched. **Connect delay** holds the bot back for up to a day before it connects, for bots that should only come online later, and a delayed connect can still be cancelled with Stop. A script that was running when the connection dropped restarts from its first step once the bot is back in.
- **Variables, conditions and branches in bot scripts** — Scripts can now remember things and act on them. **Set** stores a value under a name, **Increase** changes a number by an amount (a negative amount works too), and **If** runs one list of steps when a condition holds and a different list when it does not. Conditions can check whether the chat said something, whether a variable equals or is bigger than a value, whether a named player has joined, or whether the bot's health fell below a number. An If can also wait for its condition to come true: you give it a timeout, it keeps re-checking while it waits, chat that arrives during the window settles the branch right away, and when the time runs out the condition simply counts as false — the else branch runs instead of the script failing. Messages, commands and waits understand `{variable}` placeholders that are replaced with the variable's current value, so one script can greet whoever actually turned up. The limits that keep scripts predictable — at most 64 variables, names up to 32 characters, values up to 512 — are enforced by the editor and by the bot server alike.
- **Bot scripts as files** — The script editor has **Import...** and **Export...** buttons that save and load whole scripts as `.json` files, so a script that works can be backed up, shared and moved to another computer. Importing a script whose name already exists asks before it overwrites anything.
- **A live map of your bots** — The bot manager has a **Map** button that opens a top-down view of every connected bot. Each bot keeps the colour it has in the manager's table, leaves a fading trail of where it has been, and shows its health in the legend; the background grid is chunk-sized, so positions are easy to read off. Positions arrive about once per second per bot, the trail of a bot that disconnects is cleared, and the map keeps what it has drawn when you close and reopen it.
- **Offline capes from crafty.gg** — Offline accounts get an **Offline Cape** section in the skin manager, in place of the Mojang cape list that never applied to them. **Browse capes online...** looks up a player name and lists the capes crafty.gg tracks for that player — Mojang, OptiFine and crafty.gg's own, up to eight, each with its source shown — and **Use cape** applies one. **Import from file...** and **Import from URL...** take any cape PNG you already have, and **Remove cape** clears it again. The chosen cape is stored next to the account's skin and served to the game by the same local mechanism offline skins already use, including the addresses newer Minecraft versions accept; and because the cape's address changes whenever the cape does, a changed cape is picked up at the next launch, exactly like a changed skin.
- **The cape comes with a matching elytra** — This is also the answer to where elytra textures come from: there is no separate elytra texture to hunt down. When a player wears a cape, Minecraft paints the cape's texture onto the elytra all by itself, and the cape providers (OptiFine, LabyMod, MinecraftCapes and the rest) ship their elytra look as part of the cape. Equipping one of these capes therefore gives an offline account a matching elytra with no further setting — the cape PNG is the elytra texture.
- **Playtime and performance statistics** — Every instance has a new **Performance** page. It shows the playtime the launcher already keeps — total time played, the last session, when the instance was last launched and how many times, with a **Reset playtime** button — next to the hardware the launcher knows about (CPU, core count, RAM with how much is free right now, and the graphics cards). While the game runs it also shows the game process itself: its process ID, CPU usage, memory usage and uptime, each traced by a rolling chart of the last 90 samples. CPU usage is measured as a share of a single core, so it can go above 100% on multi-core systems. Works on Windows, macOS and Linux.
- **Sorting instances by time played** — The instance list can now be sorted by total time played, most played first, alongside by name and by last launched. It is in the launcher settings, under **Instance Sorting**.
- **Hardware-tuned Java arguments** — The performance presets window (the **Performance Presets** button in the toolbar) now shows your detected CPU cores next to the RAM, and for the presets with 3 GB of memory or more it offers **Apply hardware-tuned JVM arguments** — Aikar's flags, adjusted to the chosen memory: 8 MB heap regions at 3 GB, and 16 MB regions plus string deduplication and AlwaysPreTouch at 6 GB and up. The arguments are only written when the box is ticked, so a hand-tuned value is never silently replaced, and instances that set their own Java arguments in their settings are not affected.
- **Instance templates** — **Save as Template...**, on the instance toolbar and in the File menu, turns a configured instance into a reusable starting point: you give the template a name and an icon, and choose what to take along — mods and configs by default, optionally resource packs, shader packs and the server list. Worlds, logs, crash reports, screenshots and backups are never included. **New from Template...**, in the File menu right next to Add Instance, creates a fresh instance from one: pick the template, give the instance its own name, group and icon, and it arrives with a fresh identity and a playtime of zero, leaving the original untouched. Templates you no longer need can be deleted from that same window.
- **Updating every instance's mods in one go** — **Update All Instances...** in the File menu lists your instances with a checkbox each, checks the ticked ones for mod updates the same way the per-instance "Check for updates" does, and then installs everything it found in one run under the launcher's usual download limit. Instances that are currently running are skipped after a warning that names them; the summary counts instances with updates, up-to-date ones, ones that could not be checked and skipped ones, and each row shows its own status as the work goes along.
- **A face for every account** — Accounts without a skin used to show an empty or generic icon where their head belongs. The face now falls back in order: the skin the profile currently wears, then the skin picked for that account in the skin manager (the same one the offline skin agent serves to the game), and finally the head of the launcher's bundled default skin, in color with the hat overlay on top. The Accounts button in the main window uses that default skin head when no account is selected, replacing the old "no account" theme icon. Saving or resetting a skin also tells the account views to refresh, so the new face appears right away instead of after a restart.

**Changed:**

- **Old bot lists keep working** — Bot configs saved before this update simply have the new options off, and scripts written before run exactly as before; nothing has to be redone after updating.
- **The performance presets window shows detected CPU cores** next to the RAM it already showed, so the detected line reads like "8 cores, 15.8 GB RAM".
- **A tidier skin manager** — The button bar at the bottom of the skin manager is down to a **⋯** menu on the left and **Import...**, **Import file...** and **Browse online...** on the right, next to Close and OK. **Import...** opens a small window where you paste a skin link or type a Minecraft username, and the launcher picks the right way to fetch it. **Open Folder** and **Reset Skin** moved into the **⋯** menu.
- **Cape browser that shows capes right away** — **Browse capes** now opens with a grid of capes, like the skin browser, filling in as capes are found for the players of the skin catalog; **Load more** looks through more players, and an empty search brings the grid back. Clicking a cape shows it in the 3D preview on the skin you have selected in the skin manager, with a **Preview Elytra** checkbox, and a plain mannequin is used when no skin is selected. The window remembers its size, and **Use cape**, which previously did nothing, now applies the cape.
- **The skin manager stays open and starts on your skin** — **OK** now applies the selected skin without closing the window, so you can keep trying skins and capes; **Close** (formerly Cancel) or Esc closes it. If applying fails the window stays open with the error message. The 3D preview opens on the skin your account is currently wearing — for offline accounts that is the skin you last applied — and shows the launcher's default skin when nothing is selected. The "skin in use" check that blocks deleting a skin now recognises an offline account's skin correctly instead of marking the first skin in the list.
- **The offline cape shows on the 3D skin preview** — A cape you pick, import or remove for an offline account is drawn on the skin in the 3D preview straight away (with the elytra when **Preview Elytra** is ticked), so the small flat picture of the cape in the Offline Cape box is gone; the cape's name stays.
- **The online skin and cape browsers open on a fresh feed** — Opening **Browse online...** or **Browse capes online...** now starts on a different part of the crafty.gg catalog than last time instead of always the same first page. The feed changes at most once every 5 minutes, and only when you open the browser: open it again within 5 minutes and you see the same feed, open it after a longer break and you get a new one (a break of an hour is still just one change). Page numbers therefore start somewhere other than 1, and **Previous** still goes back; a search and the empty-search "back to the feed" behave as before.

**Notes:**

- Your instances, accounts, skins, bots, bot configs and scripts carry over exactly as they are — there is nothing to do after updating.
- Templates are ordinary folders inside a "templates" folder in the launcher's data directory, so they can be browsed, copied and backed up like any other files.

## v10.0.7

**Turns on the automatic Java download on every platform, makes updates mandatory with a check every 30 minutes, adds a way to rename offline accounts, and fixes the Arch package not starting, the unknown platform and commit in the About dialog, slow AppImage builds, and offline skins not appearing on newer Minecraft versions.**

**Fixed:**

- **Arch package not starting** — The Arch package installed fine, but the launcher failed to start with "libonnxruntime.so.1: cannot open shared object file". The library was installed in a private folder that the launcher did not search. The launcher now looks there as well.
- **Platform shown as "unknown"** — The About dialog printed `Platform: unknown` on Windows and Linux builds because only the macOS build set it. The platform is now filled in automatically as Windows, Linux or macOS.
- **Commit shown as "GITDIR-NOTFOUND"** — Builds made from a source archive or tarball have no Git history, so the About dialog printed a placeholder instead of a commit. CI now passes the real commit, and the line is hidden when it is truly unknown.
- **Updates only checked once a day** — The launcher only looked for a new version once every 24 hours, and a startup check only happened if a day had passed since the last one. It now checks on every startup and every 30 minutes.
- **Updates could be skipped** — The update window had a Skip option that hid that version for good. There is no skip option anymore.
- **AppImage builds taking up to an hour to install dependencies** — A slow Ubuntu or Qt download could stall the AppImage build for most of an hour. Downloads now time out and retry, a faster mirror is used when available, and the downloaded Qt is kept for the next build even if a later step fails.
- **Offline skins not appearing on newer Minecraft versions** — When you selected a skin for an offline account, the game ignored it and kept showing the default skin. Newer Minecraft versions check the address of every skin in your profile against a list of official addresses, and the launcher's addresses were not on that list, so the game silently threw them away. The launcher now uses addresses that pass the check; the old addresses keep working, so older game versions are not affected.

**Added:**

- **Automatic Java download on Linux** — The feature that downloads the right Java for each Minecraft version was switched off on Linux builds, including the AppImage and tarball. It is now built into every platform, and the Java settings show the auto-download option.
- **Mandatory updates** — When a new version is found, an "Update required" window appears with a single **Update now** button, then the launcher closes, updates and starts again. If a game is running, the update waits and retries every 5 minutes. The updater options in Settings are now greyed out on Windows and Linux.
- **Renaming offline accounts** — The Accounts page has a new **Edit Username** button. Before, the only way to change an offline account's name was to delete the account and add a new one, which meant starting over: the game treats the new account as a different player, and the skin selected for the old one no longer applies. When renaming, the account's UUID — the identity the game uses for your singleplayer progress and your selected skin — stays the same, so nothing is lost. The launcher refuses a name that another offline account already uses. Microsoft and authlib-injector accounts cannot be renamed, because their names belong to their login servers.
  
## v10.0.6

**Fixes the Nix package so it installs under the PollyMC name, and keeps builds working after GitHub stopped serving ONNX Runtime.**

**Fixed:**

- **Nix package still named prismlauncher** — The Nix flake installed the program as `prismlauncher`. It is now `pollymc`, and `nix profile add github:PollyMC-Continued/launcher` gives you a `pollymc` command.
- **Nix builds pulling from Prism's cache** — The flake now uses the PollyMC Cachix cache, so builds can be downloaded from there instead of compiled from scratch.
- **Builds failing with "404" on ONNX Runtime** — GitHub stopped serving the ONNX Runtime repository, so every build that downloaded it failed. Builds now get ONNX Runtime 1.22.0 from the official PyPI package.

**Added:**

- **Nix build workflow** — A new CI workflow builds the Nix package and uploads it to the PollyMC Cachix cache.

## v10.0.5

**Fixes offline skins not updating after the first time, and makes "Could not download metadata" errors tell you what actually went wrong.**

**Fixed:**

- **Offline skins only applied the first time** — After you applied a skin to an offline account, later changes were ignored, and the only workaround was creating a new account. Minecraft keeps a saved copy of each skin it downloads, filed under the skin's address, and the launcher used the same address for an account's skin every time, so the game kept showing the old copy. The address now changes whenever the skin changes, so the new skin is picked up the next time you launch.
- **"Could not download metadata" errors now explain why** — When the launcher could not download the version information for a component such as NeoForge, the error only said to try again later and never gave the reason. It now shows the actual reason, so you can tell a connection problem from a missing or damaged file.
- **Wrong components listed in metadata download errors** — When one component failed to download, the error could list every component the launcher was fetching at the time, not just the one that failed. It now names only the components that actually failed.

## v10.0.4

**Fixes the crash many Windows users hit while browsing mods, makes failed modpack imports explain what actually went wrong, and repairs the Nix build.**

**Fixed:**

- **Crash while browsing mods with the Windows 11 style ("The launcher closed unexpectedly")** — To pick a readable text colour for selected rows, the launcher painted each selected row into a picture in memory - for every selected row, on every refresh, at the row's full size. If that picture could not be created, the painting silently did nothing and the launcher went on to read from it anyway, which could close it; the "QPainter" warnings some of you found at the end of your logs are the sign of that. The colour is now measured from a small cached sample that is rendered correctly on high-resolution screens, the launcher checks that the sample could really be created before using it, and falls back to a safe colour when it could not. Lists with selected rows also refresh much faster now. Every other place that draws into a picture in memory (the sign-in QR code, the splash screen, skins and capes, screenshots, drag previews of instances) got the same safety net.
- **Modpack imports failing with only "Failed to extract file ..."** — When a file could not be written while unpacking a pack (most often because the disk ran out of space, security software locked the file, or the path was too long), the error did not say which of these it was. The message now includes the real reason, for example "Write failed: No space left on device", and the half-written file and the temporary import folder are cleaned up after a failure instead of being left behind.
- **Nix build broken on NixOS** — Building the launcher with Nix failed because the packaging had not kept up with the launcher's requirements: the Qt SVG component, the ONNX Runtime library, and the changelog that is shown inside the launcher were all missing from the package. All three are included again, so `nix build` and `nix profile add` work on NixOS once more.

**Added:**

- **Free space check before unpacking** — Before a modpack is extracted, the launcher now adds up the pack's unpacked size and compares it against the free space on the drive the launcher's data lives on. If it will not fit, you get a clear message with both numbers right away, instead of the import dying halfway through after minutes of unpacking.
- **Install instructions for NixOS** — The README now has a NixOS section explaining how to install the launcher straight from its flake or add it to your system configuration, and noting that the AppImage and tarball releases do not work out of the box on NixOS.

**Changed:**

- **Helpful steps right in the error window for failed modpack imports** — When a modpack cannot be written to disk (a full disk, security software, a path that is too long), the error window now shows the practical steps right away, and the "Report Bug" button is hidden for that case because it is not a launcher bug. If the advice does not help, a report can still be sent from the launcher's menu.
- **The Nix package is now built automatically** — Whenever the files that affect Nix packaging change, the launcher's CI builds the Nix package, so NixOS breakage is caught before it reaches a release.

## v10.0.3

**No more "the launcher closed unexpectedly" after shutting down or restarting your computer, and clearer messages when something on your side - not the launcher - goes wrong.**

**Fixed:**

- **"The launcher closed unexpectedly" after a shutdown, restart or power loss** — The launcher kept a small marker file while it ran and removed it when it closed normally; if the marker was still there on the next start, it assumed it had crashed. But when Windows shuts down, the machine loses power or you log off while the launcher is open, the launcher never gets the chance to clean up - so the next start wrongly asked you to send a crash report. The launcher now also removes the marker when the system ends the session, and the marker records when the computer was last booted: if that changed, the previous run ended with a shutdown or power loss, which is logged quietly instead of being treated as a crash. An old marker from an earlier version is simply ignored.
- **Crash prompt asking twice about one event** — the leftover marker is now removed before the prompt can be shown again for the same incident.

**Added:**

- **Friendlier messages for problems that are not launcher bugs** — When downloads fail because a server cannot be reached, the error window now says so in one sentence - which server, and that this is usually the internet connection, a firewall or antivirus program, a provider or regional block, or the server being down - with a short list of things to try (another network or a VPN, the proxy settings, temporarily turning off antivirus filtering, opening the server's address in a browser). The full list of failed downloads is one click away behind "Show details". Similar notes appear for "Permission denied" (security software), full or read-only disks, and secure connection problems (often a wrong system clock). In these cases the "Report Bug" button is hidden, since these are not problems the developers can fix; the report window still shows the same advice with a way to send the report anyway.
- **Setting to turn off the crash prompt** — "Offer to report a crash after an unexpected exit" in the launcher settings. Real crashes are still asked about; shutdowns and power losses never are.

**Changed:**

- **Downloads survive flaky connections better** — When most of a download job's requests time out, the remaining retries run at most two at a time and wait longer between attempts, instead of hammering a struggling connection. The default retry limit before the error window appears is now 3 instead of 1.
- **Better judgement of what is a crash** — A leftover marker alone no longer means "crash": the previous session's log is checked for real crash evidence, and only a recent unclean exit on the same boot still counts. A stale or unreadable marker is ignored.

## v10.0.2

**Fixes a crash when closing the Modrinth or CurseForge pages while their filter lists were still loading, and two smaller issues found in the same code.**

**Fixed:**

- **Crash when closing the Modrinth or CurseForge pages while their filters load** — The Modrinth and CurseForge pages of the "New Instance" window and of an instance's "Download mods" window fetch their filter category lists in the background when opened. If you closed the page before the list arrived (easy to do on a slow connection), the finished request could still call back into a page that no longer existed, and the launcher could crash. The category work is now tied to the page's lifetime: once the page closes, a pending category list can no longer touch it.
- **Garbled log line when a category list could not be read** — If the Modrinth or CurseForge category list arrived but could not be read as JSON, the log printed a single stray character (the first one of the response) instead of the response itself. The log now shows what was actually received, which makes such problems much easier to diagnose.
- **A leftover search timer** — When a mod page's filters were set up a second time, the old "wait a moment before searching" timer was left alive next to the new one, and a search that was already waiting could fire on top of the new one. The old timer is now removed before the new one is created. (In practice the filters are set up only once per page, so you were unlikely to notice.)

## v10.0.1

**Fixes "Check instance" false alarms for bundled libraries, detects version mismatches, and improves "Update Pack" reliability.**

**Added:**

- **Dependency version mismatches detected** — "Check instance" now reports an error when a required mod is installed, but its version is outside the required range (e.g., a mod needs `[1.3.8, 1.4)` but `21.1.1` is installed). Previously, the launcher only checked if the mod was present, letting the game crash on startup.
- **Pack file verification** — "Check instance" now compares your actual instance against the modpack's file list. It warns you about pack files missing from your instance and flags "orphan" mods (jars in your mods folder that are not part of the pack, often left behind by previous updates or added manually).

**Fixed:**

- **Mod conflict false alarms with bundled libraries** — Many modpacks, including All the Mods 10: To the Sky, ship libraries inside other mods (NeoForge's "Jar-in-Jar"). "Check instance" no longer reports those libraries as possibly missing when another installed mod already includes them. Real missing dependencies are still reported.
- **Mods that only contain other mods** — Library-style mods that carry their mods inside themselves (such as Kotlin for Forge) are now recognized and counted as installed, instead of being skipped.
- **Update Pack ignoring changed or deleted files** — Modrinth and CurseForge updates now verify that "kept" files actually still exist on your disk. If you updated or deleted a mod individually outside the pack, "Update Pack" will now detect it, warn you, and restore the pack's version. Intentionally disabled mods are still respected and left alone.
- **CurseForge "Update Pack" leaving duplicate mods behind** — After an update, an instance could end up with two versions of the same mod (for example two Ars Nouveau or GeckoLib jars), and the game refused to start. If you had updated a mod yourself (for example with "Check for Updates"), "Update Pack" would also download the pack's old copy of it again next to yours. "Update Pack" now notices when a mod was replaced by another version of the same mod, no longer downloads the pack's file a second time, and removes extra copies of mods the pack already provides. The removed files are listed in the log.
- **CurseForge "Update Pack" hiding failures** — If the launcher could not get the old file names from CurseForge, or could not read the previous pack's file list, the update went on silently and could not clean up old files. It now asks you before continuing, and tells you in the log when some old files could not be identified.
- **Stale configs not overwriting** — Fixed a bug on Windows where updating a pack would silently fail to overwrite existing config files and overrides, leaving you with outdated settings. Files are now verified and removed before copying.
- **Override cleanup failing** — Fixed a bug where certain config files were never deleted between pack updates if their path contained the word "overrides" (e.g., `overrides/config/overrides.cfg`).
- **Crash when switching to the Windows 11 style** — On Windows, changing the widget style from Dark (or another custom theme) to "Windows 11" could close the launcher right after the change. The launcher now clears the leftover theme before applying the Windows 11 style, so the switch completes.

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
