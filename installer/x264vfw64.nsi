;x264vfw - H.264/MPEG-4 AVC codec for x64
;Written by BugMaster

!define FullName  "x264vfw - H.264/MPEG-4 AVC codec for x64"
!define ShortName "x264vfw64"

;--------------------------------
;Includes

  !include "MUI.nsh"
  !include "Sections.nsh"
  !include "WinVer.nsh"
  !include "x64.nsh"

;--------------------------------
;General

  SetCompressor /SOLID lzma

  ;Name and file
  Name "${FullName}"
  OutFile "${ShortName}.exe"

  ;Default installation folder
  InstallDir "$SYSDIR"

;--------------------------------
;Interface Configuration

  !define MUI_HEADERIMAGE
  !define MUI_COMPONENTSPAGE_NODESC
  !define MUI_FINISHPAGE_NOAUTOCLOSE
  !define MUI_UNFINISHPAGE_NOAUTOCLOSE
  !define MUI_ABORTWARNING

;--------------------------------
;Pages

  !define MUI_WELCOMEPAGE_TITLE_3LINES
  !insertmacro MUI_PAGE_WELCOME
  !insertmacro MUI_PAGE_INSTFILES
  !define MUI_FINISHPAGE_TITLE_3LINES
  !insertmacro MUI_PAGE_FINISH

  !insertmacro MUI_UNPAGE_CONFIRM
  !insertmacro MUI_UNPAGE_INSTFILES
  !define MUI_FINISHPAGE_TITLE_3LINES
  !insertmacro MUI_UNPAGE_FINISH

;--------------------------------
;Languages

  !insertmacro MUI_LANGUAGE "English"

;--------------------------------
;Installer Sections

Section "-Required"

  ;Create uninstaller
  WriteUninstaller "$INSTDIR\${ShortName}-uninstall.exe"

  ; Write the uninstall keys for Windows
  WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${ShortName}" "UninstallString" "$INSTDIR\${ShortName}-uninstall.exe"
  WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${ShortName}" "DisplayIcon" "$INSTDIR\${ShortName}-uninstall.exe"
  WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${ShortName}" "DisplayName" "${FullName} (remove only)"
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${ShortName}" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${ShortName}" "NoRepair" 1

  ${DisableX64FSRedirection}
  SetOutPath "$SYSDIR"
  File ".\x264vfw64.dll"
  ${EnableX64FSRedirection}
  SetOutPath "$INSTDIR"
  File /oname=x264vfw64.ico ".\x264vfw.ico"

  CreateDirectory $SMPROGRAMS\${ShortName}

  ${DisableX64FSRedirection}
  IfFileExists "$SYSDIR\rundll32.exe" RUNDLL32_SYSDIR RUNDLL32_WINDIR
RUNDLL32_WINDIR:
  SetOutPath "$WINDIR"
  CreateShortcut "$SMPROGRAMS\${ShortName}\Configure ${ShortName}.lnk" "$WINDIR\rundll32.exe" "x264vfw64.dll,Configure" "$INSTDIR\x264vfw64.ico"
  SetOutPath "$INSTDIR"
  Goto RUNDLL32_end
RUNDLL32_SYSDIR:
  SetOutPath "$SYSDIR"
  CreateShortcut "$SMPROGRAMS\${ShortName}\Configure ${ShortName}.lnk" "$SYSDIR\rundll32.exe" "x264vfw64.dll,Configure" "$INSTDIR\x264vfw64.ico"
  SetOutPath "$INSTDIR"
RUNDLL32_end:
  ${EnableX64FSRedirection}

  CreateShortcut "$SMPROGRAMS\${ShortName}\Uninstall ${ShortName}.lnk" "$INSTDIR\${ShortName}-uninstall.exe"

  SetRegView 64

  ${If} ${IsNT}
    WriteRegStr HKLM "Software\Microsoft\Windows NT\CurrentVersion\drivers32"    "vidc.x264"    "x264vfw64.dll"
    WriteRegStr HKLM "Software\Microsoft\Windows NT\CurrentVersion\drivers.desc" "x264vfw64.dll"  "${FullName}"
  ${Else}
    WriteINIStr "$WINDIR\system.ini" "drivers32" "vidc.x264" "x264vfw64.dll"
    WriteRegStr HKLM "System\CurrentControlSet\Control\MediaResources\icm\vidc.x264" "Driver"       "x264vfw64.dll"
    WriteRegStr HKLM "System\CurrentControlSet\Control\MediaResources\icm\vidc.x264" "Description"  "${FullName}"
    WriteRegStr HKLM "System\CurrentControlSet\Control\MediaResources\icm\vidc.x264" "FriendlyName" "${FullName}"
  ${EndIf}

  DeleteRegKey HKCU "Software\GNU\x264vfw64"

  SetRegView lastused

SectionEnd

;--------------------------------
;Installer Functions

Function .onInit

  ${If} ${RunningX64}
    StrCpy $INSTDIR "$WINDIR\SysWOW64"
  ${Else}
    MessageBox MB_OK|MB_ICONEXCLAMATION "This program could be installed only on x64 version of Windows"
    Abort
  ${EndIf}

  ; Check Administrator's rights
  ClearErrors
  UserInfo::GetAccountType
  IfErrors not_admin
  Pop $R0
  StrCmp $R0 "Admin" admin
not_admin:
  ; No Administrator's rights
  MessageBox MB_YESNO|MB_ICONEXCLAMATION "For correct installation of this program you must have $\"Administrator's rights$\"!$\r$\nDo you wish to continue installation anyway?" \
    /SD IDNO IDYES admin
  Abort
admin:

  ClearErrors

FunctionEnd

;--------------------------------
;Uninstaller Section

Section "Uninstall"

  SetRegView 64
  Push $R0

  ReadRegStr $R0 HKLM "Software\Microsoft\Windows NT\CurrentVersion\drivers32" "vidc.x264"
  DeleteRegValue HKLM "Software\Microsoft\Windows NT\CurrentVersion\drivers.desc" $R0
  DeleteRegValue HKLM "Software\Microsoft\Windows NT\CurrentVersion\drivers32" "vidc.x264"
  DeleteINIStr "$WINDIR\system.ini" "drivers32" "vidc.x264"
  DeleteRegKey HKLM "System\CurrentControlSet\Control\MediaResources\icm\vidc.x264"
  ${DisableX64FSRedirection}
  Delete /REBOOTOK "$SYSDIR\x264vfw64.dll"
  ${EnableX64FSRedirection}
  Delete /REBOOTOK "$INSTDIR\x264vfw64.ico"

  Pop $R0
  SetRegView lastused

  Delete /REBOOTOK "$SMPROGRAMS\${ShortName}\Configure ${ShortName}.lnk"
  Delete /REBOOTOK "$SMPROGRAMS\${ShortName}\Uninstall ${ShortName}.lnk"
  RMDir  /REBOOTOK "$SMPROGRAMS\${ShortName}"

  Delete /REBOOTOK "$INSTDIR\${ShortName}-uninstall.exe"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${ShortName}"

SectionEnd

;--------------------------------
;Uninstaller Functions

Function un.onInit

  StrCpy $INSTDIR "$SYSDIR"

  ${If} ${RunningX64}
    StrCpy $INSTDIR "$WINDIR\SysWOW64"
  ${EndIf}

FunctionEnd
