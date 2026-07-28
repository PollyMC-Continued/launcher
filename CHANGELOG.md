# Changelog

## v9.0.10

**Added:**
- Yggdrasil (authlib-injector) account support
  - New AccountType::AuthlibInjector
  - AuthlibInjectorStep for /authserver/authenticate and /authserver/refresh
  - Login dialog with server URL, username, password
  - JVM agent injection for authlib-injector at launch
  - "Add Yggdrasil" toolbar button
- macOS derives version from git tag (like Windows/Linux)

**Fixed:**
- Use accountData() accessor instead of protected data member

**Removed:**
- Unused accountIsOnline variable

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

**Fixed:**
- Use --codesigning=off for Qt 6.9+ macdeployqt
- Remove codesign flags for Qt 6.9+ macdeployqt
- verify_bundle.sh for Qt 6.11: handle @rpath/Frameworks and bundle-internal framework deps
- Skip binary files in branch name grep to avoid matching 'main' in compiled code

**Changed:**
- Revise README for clarity and feature highlights
- Shorten README

## v9.0.8

**Added:**
- Dll Checks in Github Action
- Enhance build script with DLL checks and updates
- Enhance release workflow with additional triggers
- Enhance versioning logic in build.yml

**Fixed:**
- Filter Windows system DLLs from recursive dep check; add installer DLL test step
- Add msvcp_win.dll to Windows system DLL skip list
- Replace libgamemode-dev with gamemode-dev for Ubuntu 24.04
- Cache linuxdeploy step with proper key and restore-keys
- Formatting and error messages in build.yml
- Formatting issue in build.yml
- GitHub actions workflow bugs (×2)
- Icon naming in .desktop file

**Changed:**
- Refactor CI workflow for Windows and Linux builds
- Refactor DLL dependency handling in build workflow
- Refactor CMake build process and improve logging
- Refactor deployment script for improved clarity
- Update build permissions and fix version handling
- Update release workflow to handle versioning
- Improve DLL deployment logic

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

**Fixed:**
- Deploy MinGW runtime DLLs in Windows build
- Resolve release conflict in CI workflow
- Let GitHub auto-generate release notes from commits

**Changed:**
- Update CI workflow
- Update pollymc icon, remove old files

**Other:**
- Debug: Verify release files exist before upload
