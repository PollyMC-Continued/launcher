!include "MUI2.nsh"

; BUILD_DIR can be passed on the command line: makensis /DBUILD_DIR=C:\build pollymc_installer.nsi
!ifndef BUILD_DIR
    !define BUILD_DIR "C:\pollymc_build"
!endif
; VERSION is passed by CI; keep a fallback for local builds
!ifndef VERSION
    !define VERSION "9.0.0"
!endif

Name "PollyMC-Continued"
OutFile "PollyMC-Continued-9.0.0-Setup.exe"
InstallDir "$PROGRAMFILES64\PollyMC-Continued"
RequestExecutionLevel admin

; A locked file must not be skippable: "Ignore" used to leave a half-old, half-new install.
AllowSkipFiles off

Var IsUpgrade

Function .onInit
    StrCpy $IsUpgrade 0
    ReadRegStr $0 HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\PollyMC-Continued" "UninstallString"
    StrCmp $0 "" done

    ; Old installation found - ask to upgrade
    MessageBox MB_YESNO "PollyMC-Continued is already installed. Do you want to upgrade?$\n$\nYour settings and instances will be kept." IDYES upgrade
    Abort

    upgrade:
        StrCpy $IsUpgrade 1
        ; Upgrade where it is actually installed (custom $INSTDIR is remembered in the registry)
        ReadRegStr $0 HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\PollyMC-Continued" "InstallLocation"
        StrCmp $0 "" done
        StrCpy $INSTDIR $0
    done:
FunctionEnd

; Runs from the install section (not .onInit), so cancelling the installer never deletes anything.
Function CleanOldInstall
    ; Stop everything that can hold files in the install folder. No /T: the installer itself is a
    ; child of the updater that started it. Each nsExec call pushes exit code + output.
    nsExec::ExecToStack 'taskkill /F /IM pollymc.exe'
    Pop $0
    Pop $0
    nsExec::ExecToStack 'taskkill /F /IM pollymc_updater.exe'
    Pop $0
    Pop $0
    nsExec::ExecToStack 'taskkill /F /IM pollymc_filelink.exe'
    Pop $0
    Pop $0

    ; Never wipe a portable install or any other folder holding user data
    IfFileExists "$INSTDIR\portable.txt" cleaned
    IfFileExists "$INSTDIR\UserData\*.*" cleaned
    IfFileExists "$INSTDIR\instances\*.*" cleaned

    ; Killed processes release their file handles a moment later: retry for up to ~15 s
    StrCpy $1 0
    wipe:
        RMDir /r "$INSTDIR"
        IfFileExists "$INSTDIR\*.exe" 0 cleaned
        IntOp $1 $1 + 1
        IntCmp $1 15 cleaned 0 cleaned
        Sleep 1000
        Goto wipe
    cleaned:
FunctionEnd

!define MUI_ICON "program_info\pollymc.ico"
!define MUI_UNICON "program_info\pollymc.ico"

!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_RUN "$INSTDIR\pollymc.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Launch PollyMC-Continued"
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Section "Install"
    StrCmp $IsUpgrade 1 0 +2
        Call CleanOldInstall
    SetOutPath "$INSTDIR"
    File /r "${BUILD_DIR}\*.*"

    CreateDirectory "$SMPROGRAMS\PollyMC-Continued"
    CreateShortcut "$SMPROGRAMS\PollyMC-Continued\PollyMC-Continued.lnk" "$INSTDIR\pollymc.exe"
    CreateShortcut "$SMPROGRAMS\PollyMC-Continued\Uninstall.lnk" "$INSTDIR\uninstall.exe"
    IfSilent skip_shortcut
    MessageBox MB_YESNO "Create a desktop shortcut for PollyMC-Continued?" IDNO skip_shortcut
    CreateShortcut "$DESKTOP\PollyMC-Continued.lnk" "$INSTDIR\pollymc.exe"
    skip_shortcut:

    WriteUninstaller "$INSTDIR\uninstall.exe"

    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\PollyMC-Continued" "DisplayName" "PollyMC-Continued"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\PollyMC-Continued" "UninstallString" "$\"$INSTDIR\uninstall.exe$\""
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\PollyMC-Continued" "InstallLocation" "$INSTDIR"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\PollyMC-Continued" "DisplayVersion" "${VERSION}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\PollyMC-Continued" "Publisher" "PollyMC-Continued Contributors"
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\PollyMC-Continued" "NoModify" 1
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\PollyMC-Continued" "NoRepair" 1
SectionEnd

Section "Uninstall"
    RMDir /r "$INSTDIR"
    Delete "$DESKTOP\PollyMC-Continued.lnk"
    RMDir /r "$SMPROGRAMS\PollyMC-Continued"
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\PollyMC-Continued"
SectionEnd
