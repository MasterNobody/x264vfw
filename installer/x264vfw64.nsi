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
  InstallDir ""

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
  !insertmacro MUI_PAGE_LICENSE "..\COPYING"
  !insertmacro MUI_PAGE_DIRECTORY
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

  ${DisableX64FSRedirection}

  SetOutPath $INSTDIR

  ;Create uninstaller
  WriteUninstaller "$INSTDIR\${ShortName}-uninstall.exe"

  ; Write the uninstall keys for Windows
  WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${ShortName}" "UninstallString" "$INSTDIR\${ShortName}-uninstall.exe"
  WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${ShortName}" "DisplayIcon" "$INSTDIR\${ShortName}-uninstall.exe"
  WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${ShortName}" "DisplayName" "${FullName} (remove only)"
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${ShortName}" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${ShortName}" "NoRepair" 1

  File ".\x264vfw64.dll"
  File /oname=x264vfw64.ico ".\x264vfw.ico"

  CreateDirectory "$SMPROGRAMS\${ShortName}"

  IfFileExists "$SYSDIR\rundll32.exe" RUNDLL32_SYSDIR RUNDLL32_NOT_SYSDIR
RUNDLL32_SYSDIR:
  CreateShortcut "$SMPROGRAMS\${ShortName}\Configure ${ShortName}.lnk" "$SYSDIR\rundll32.exe" "x264vfw64.dll,Configure" "$INSTDIR\x264vfw64.ico"
  Goto RUNDLL32_end
RUNDLL32_NOT_SYSDIR:
  CreateShortcut "$SMPROGRAMS\${ShortName}\Configure ${ShortName}.lnk" "rundll32.exe" "x264vfw64.dll,Configure" "$INSTDIR\x264vfw64.ico"
RUNDLL32_end:

  CreateShortcut "$SMPROGRAMS\${ShortName}\Uninstall ${ShortName}.lnk" "$INSTDIR\${ShortName}-uninstall.exe"

  SetRegView 64

  GetFullPathName /SHORT $R0 "$INSTDIR\x264vfw64.dll"
  ${If} ${IsNT}
    WriteRegStr HKLM "Software\Microsoft\Windows NT\CurrentVersion\drivers32"    "vidc.x264"    $R0
    WriteRegStr HKLM "Software\Microsoft\Windows NT\CurrentVersion\drivers.desc" $R0            "${FullName}"
  ${Else}
    WriteINIStr "$WINDIR\system.ini" "drivers32" "vidc.x264" $R0
    WriteRegStr HKLM "System\CurrentControlSet\Control\MediaResources\icm\vidc.x264" "Driver"       $R0
    WriteRegStr HKLM "System\CurrentControlSet\Control\MediaResources\icm\vidc.x264" "Description"  "${FullName}"
    WriteRegStr HKLM "System\CurrentControlSet\Control\MediaResources\icm\vidc.x264" "FriendlyName" "${FullName}"
  ${EndIf}

  SetRegView lastused

  ${EnableX64FSRedirection}

SectionEnd

;--------------------------------
;Installer Functions

Function .onInit

  ${IfNot} ${RunningX64}
    MessageBox MB_OK|MB_ICONEXCLAMATION "This program could be installed only on x64 version of Windows" /SD IDOK
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

  ReadRegStr $R0 HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${ShortName}" "UninstallString"
  ; GetParent
  StrCpy $R1 0
  IntOp  $R1 $R1 - 1
  StrCpy $R2 $R0 1 $R1
  StrCmp $R2 '\' +2
  StrCmp $R2 ''  +1 -3
  IntOp  $R1 $R1 + 1
  StrCpy $R1 $R0 $R1
  ; Was a previous installation inside the $WINDIR?
  StrLen $R2 "$WINDIR\"
  StrCpy $R2 $R0 $R2
  StrCmp $R2 "$WINDIR\" uninstall_ask uninstall_not_needed
uninstall_ask:
  MessageBox MB_YESNO|MB_ICONQUESTION "A previous installation of ${ShortName} has been detected inside the Windows directory.$\r$\nDo you wish to uninstall it? (recommended)" \
    /SD IDYES IDNO uninstall_not_needed
  GetTempFileName $R2
  CopyFiles $R0 $R2
  ExecWait '"$R2" /S _?=$R1'
  Delete $R2
  StrCpy $R1 ""
uninstall_not_needed:
  StrCmp $INSTDIR "" +1 +2
  StrCpy $INSTDIR $R1
  StrCmp $INSTDIR "" +1 +2
  StrCpy $INSTDIR "$PROGRAMFILES64\${ShortName}\"

FunctionEnd

;--------------------------------
;Uninstaller Section

Section "Uninstall"

  ${DisableX64FSRedirection}

  SetRegView 64

  MessageBox MB_YESNO|MB_ICONQUESTION "Keep ${ShortName}'s settings in registry?" /SD IDYES IDYES keep_settings
  DeleteRegKey HKCU "Software\GNU\x264vfw64"
keep_settings:

  ReadRegStr $R0 HKLM "Software\Microsoft\Windows NT\CurrentVersion\drivers32" "vidc.x264"
  DeleteRegValue HKLM "Software\Microsoft\Windows NT\CurrentVersion\drivers.desc" $R0
  DeleteRegValue HKLM "Software\Microsoft\Windows NT\CurrentVersion\drivers32" "vidc.x264"
  DeleteINIStr "$WINDIR\system.ini" "drivers32" "vidc.x264"
  DeleteRegKey HKLM "System\CurrentControlSet\Control\MediaResources\icm\vidc.x264"

  SetRegView lastused

  Delete /REBOOTOK "$INSTDIR\x264vfw64.dll"
  Delete /REBOOTOK "$INSTDIR\x264vfw64.ico"

  Delete /REBOOTOK "$SMPROGRAMS\${ShortName}\Configure ${ShortName}.lnk"
  Delete /REBOOTOK "$SMPROGRAMS\${ShortName}\Uninstall ${ShortName}.lnk"
  RMDir  /REBOOTOK "$SMPROGRAMS\${ShortName}"

  Delete /REBOOTOK "$INSTDIR\${ShortName}-uninstall.exe"
  RMDir  $INSTDIR
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${ShortName}"

  ${EnableX64FSRedirection}

SectionEnd
