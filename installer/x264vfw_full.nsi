;x264vfw - H.264/MPEG-4 AVC codec
;Written by BugMaster

!define FullName    "x264vfw - H.264/MPEG-4 AVC codec"
!define ShortName   "x264vfw"
!define ShortName64 "x264vfw64"

;--------------------------------
;Includes

  !include "MUI.nsh"
  !include "Sections.nsh"
  !include "WinVer.nsh"
  !include "x64.nsh"
  !include "FileFunc.nsh"

;--------------------------------
;General

  SetCompressor /SOLID lzma

  ;Name and file
  Name "${FullName}"
  OutFile "${ShortName}_full.exe"

  ;Default installation folder
  InstallDir ""

  ;System32 or SysWOW64 dir according to OS
  Var SYSDIR_32bit

  ;Previous version installation folder
  Var PREV_INSTDIR
  Var PREV_INSTDIR64

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
  !insertmacro MUI_PAGE_COMPONENTS
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
;Macros

!macro DisableSection SECTION

  Push $R0
  Push $R1
  Push $R2
    StrCpy $R2 "${SECTION}"
    SectionGetFlags $R2 $R0
    IntOp $R1 "${SF_SELECTED}" ~
    IntOp $R0 $R0 & $R1
    IntOp $R0 $R0 | "${SF_RO}"
    SectionSetFlags $R2 $R0
  Pop $R2
  Pop $R1
  Pop $R0

!macroend

!macro RegCodec CODEC_ID CODEC_NAME CODEC_DESC

  Push $R0
  Push $R1
  Push $R2
    StrCpy $R0 "${CODEC_ID}"
    StrCpy $R1 "${CODEC_NAME}"
    StrCpy $R2 "${CODEC_DESC}"
    ${If} ${IsNT}
      WriteRegStr HKLM "Software\Microsoft\Windows NT\CurrentVersion\drivers32"    $R0 $R1
      WriteRegStr HKLM "Software\Microsoft\Windows NT\CurrentVersion\drivers.desc" $R1 $R2
    ${Else}
      WriteINIStr "$WINDIR\system.ini" "drivers32" $R0 $R1
      WriteRegStr HKLM "System\CurrentControlSet\Control\MediaResources\icm\$R0" "Driver"       $R1
      WriteRegStr HKLM "System\CurrentControlSet\Control\MediaResources\icm\$R0" "Description"  $R2
      WriteRegStr HKLM "System\CurrentControlSet\Control\MediaResources\icm\$R0" "FriendlyName" $R2
    ${EndIf}
  Pop $R2
  Pop $R1
  Pop $R0

!macroend

!macro UnRegCodec CODEC_ID

  Push $R0
  Push $R1
    StrCpy $R0 "${CODEC_ID}"
    ${If} ${IsNT}
      ReadRegStr $R1 HKLM "Software\Microsoft\Windows NT\CurrentVersion\drivers32"    $R0
      DeleteRegValue HKLM "Software\Microsoft\Windows NT\CurrentVersion\drivers.desc" $R1
      DeleteRegValue HKLM "Software\Microsoft\Windows NT\CurrentVersion\drivers32"    $R0
    ${Else}
      DeleteINIStr "$WINDIR\system.ini" "drivers32" $R0
      DeleteRegKey HKLM "System\CurrentControlSet\Control\MediaResources\icm\$R0"
    ${EndIf}
  Pop $R1
  Pop $R0

!macroend

;--------------------------------
;Installer Sections

Section "Remove previous version" SEC_REMOVE_PREV

  ${If} $PREV_INSTDIR != ""
    ReadRegDWORD $R0 HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${ShortName}" "Installed_x264vfw_x86"
    StrCmp $R0 "" +1 +2
    StrCpy $R0 1

    ${If} $R0 <> 0
      Delete "$SYSDIR_32bit\x264vfw.dll"

      SetShellVarContext all
      Delete "$SMPROGRAMS\${ShortName}\Configure ${ShortName}.lnk"
      SetShellVarContext current
      Delete "$SMPROGRAMS\${ShortName}\Configure ${ShortName}.lnk"

      Delete "$PREV_INSTDIR\x264vfw.dll"
      Delete "$PREV_INSTDIR\x264vfw.ico"

      !insertmacro UnRegCodec "vidc.x264"
    ${EndIf}

    ReadRegDWORD $R0 HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${ShortName}" "Installed_x264vfw_x64"
    StrCmp $R0 "" +1 +2
    StrCpy $R0 0

    ${If} $R0 <> 0
      ${If} ${RunningX64}
        ${DisableX64FSRedirection}

        Delete "$SYSDIR\x264vfw64.dll"

        SetShellVarContext all
        Delete "$SMPROGRAMS\${ShortName}\Configure ${ShortName64}.lnk"
        SetShellVarContext current
        Delete "$SMPROGRAMS\${ShortName}\Configure ${ShortName64}.lnk"

        ${EnableX64FSRedirection}

        Delete "$PREV_INSTDIR\x264vfw64.dll"
        Delete "$PREV_INSTDIR\x264vfw64.ico"

        SetRegView 64
        !insertmacro UnRegCodec "vidc.x264"
        SetRegView lastused
      ${EndIf}
    ${EndIf}

    SetShellVarContext all
    Delete "$SMPROGRAMS\${ShortName}\Uninstall ${ShortName}.lnk"
    RMDir  "$SMPROGRAMS\${ShortName}"
    SetShellVarContext current
    Delete "$SMPROGRAMS\${ShortName}\Uninstall ${ShortName}.lnk"
    RMDir  "$SMPROGRAMS\${ShortName}"

    Delete "$PREV_INSTDIR\${ShortName}-uninstall.exe"
    RMDir  $PREV_INSTDIR
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${ShortName}"
  ${EndIf}

  ${If} $PREV_INSTDIR64 != ""
    ${If} ${RunningX64}
      ${DisableX64FSRedirection}

      Delete "$SYSDIR\x264vfw64.dll"

      SetShellVarContext all
      Delete "$SMPROGRAMS\${ShortName64}\Configure ${ShortName64}.lnk"
      SetShellVarContext current
      Delete "$SMPROGRAMS\${ShortName64}\Configure ${ShortName64}.lnk"

      Delete "$PREV_INSTDIR64\x264vfw64.dll"
      Delete "$PREV_INSTDIR64\x264vfw64.ico"

      SetRegView 64
      !insertmacro UnRegCodec "vidc.x264"
      SetRegView lastused

      SetShellVarContext all
      Delete "$SMPROGRAMS\${ShortName64}\Uninstall ${ShortName64}.lnk"
      RMDir  "$SMPROGRAMS\${ShortName64}"
      SetShellVarContext current
      Delete "$SMPROGRAMS\${ShortName64}\Uninstall ${ShortName64}.lnk"
      RMDir  "$SMPROGRAMS\${ShortName64}"

      Delete "$PREV_INSTDIR64\${ShortName64}-uninstall.exe"
      RMDir  $PREV_INSTDIR64
      DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${ShortName64}"

      ${EnableX64FSRedirection}
    ${EndIf}
  ${EndIf}

SectionEnd

Section "-Install"

  SetOutPath $INSTDIR

  ;Create uninstaller
  WriteUninstaller "$INSTDIR\${ShortName}-uninstall.exe"

  ; Write the uninstall keys for Windows
  WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${ShortName}" "UninstallString" "$INSTDIR\${ShortName}-uninstall.exe"
  WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${ShortName}" "DisplayIcon" "$INSTDIR\${ShortName}-uninstall.exe"
  WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${ShortName}" "DisplayName" "${FullName} (remove only)"
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${ShortName}" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${ShortName}" "NoRepair" 1

  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${ShortName}" "Installed_x264vfw_x86" 0
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${ShortName}" "Installed_x264vfw_x64" 0

  SetShellVarContext all
  CreateDirectory "$SMPROGRAMS\${ShortName}"
  CreateShortcut  "$SMPROGRAMS\${ShortName}\Uninstall ${ShortName}.lnk" "$INSTDIR\${ShortName}-uninstall.exe"

SectionEnd

Section "${ShortName} (x86)" SEC_INSTALL_X86

  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${ShortName}" "Installed_x264vfw_x86" 1

  SetOutPath $INSTDIR
  File ".\x264vfw.ico"

  SetOutPath $SYSDIR_32bit
  File ".\x264vfw.dll"

  SetShellVarContext all
  ${If} ${FileExists} "$SYSDIR_32bit\rundll32.exe"
    CreateShortcut "$SMPROGRAMS\${ShortName}\Configure ${ShortName}.lnk" "$SYSDIR_32bit\rundll32.exe" "x264vfw.dll,Configure" "$INSTDIR\x264vfw.ico"
  ${Else}
    CreateShortcut "$SMPROGRAMS\${ShortName}\Configure ${ShortName}.lnk" "rundll32.exe" "x264vfw.dll,Configure" "$INSTDIR\x264vfw.ico"
  ${EndIf}

  !insertmacro RegCodec "vidc.x264" "x264vfw.dll" "${FullName}"

SectionEnd

Section "${ShortName} (x64)" SEC_INSTALL_X64

  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${ShortName}" "Installed_x264vfw_x64" 1

  SetOutPath $INSTDIR
  File /oname=x264vfw64.ico ".\x264vfw.ico"

  ${DisableX64FSRedirection}

  SetOutPath $SYSDIR
  File ".\x264vfw64.dll"

  SetShellVarContext all
  ${If} ${FileExists} "$SYSDIR\rundll32.exe"
    CreateShortcut "$SMPROGRAMS\${ShortName}\Configure ${ShortName64}.lnk" "$SYSDIR\rundll32.exe" "x264vfw64.dll,Configure" "$INSTDIR\x264vfw64.ico"
  ${Else}
    CreateShortcut "$SMPROGRAMS\${ShortName}\Configure ${ShortName64}.lnk" "rundll32.exe" "x264vfw64.dll,Configure" "$INSTDIR\x264vfw64.ico"
  ${EndIf}

  ${EnableX64FSRedirection}

  SetRegView 64
  !insertmacro RegCodec "vidc.x264" "x264vfw64.dll" "${FullName}"
  SetRegView lastused

SectionEnd

;--------------------------------
;Installer Functions

Function .onInit

  ${If} ${RunningX64}
    StrCpy $SYSDIR_32bit "$WINDIR\SysWOW64"
  ${Else}
    StrCpy $SYSDIR_32bit $SYSDIR
    !insertmacro DisableSection "${SEC_INSTALL_X64}"
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

  StrCpy $PREV_INSTDIR ""
  ReadRegStr $R0 HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${ShortName}" "UninstallString"
  ${GetParent} $R0 $R1
  StrCmp $R1 "" +2
  StrCpy $PREV_INSTDIR $R1

  StrCpy $PREV_INSTDIR64 ""
  ReadRegStr $R0 HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${ShortName64}" "UninstallString"
  ${GetParent} $R0 $R1
  StrCmp $R1 "" +2
  StrCpy $PREV_INSTDIR64 $R1

  StrCmp $INSTDIR "" +1 +4
  StrCpy $INSTDIR $PREV_INSTDIR
  StrCmp $INSTDIR "" +1 +2
  StrCpy $INSTDIR "$PROGRAMFILES32\${ShortName}"

  ${If} $PREV_INSTDIR == ""
    ${If} $PREV_INSTDIR64 == ""
      !insertmacro DisableSection "${SEC_REMOVE_PREV}"
    ${EndIf}
  ${EndIf}

FunctionEnd

;--------------------------------
;Uninstaller Section

Section "Uninstall"

  ReadRegDWORD $R8 HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${ShortName}" "Installed_x264vfw_x86"
  StrCmp $R8 "" +1 +2
  StrCpy $R8 1

  ReadRegDWORD $R9 HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${ShortName}" "Installed_x264vfw_x64"
  StrCmp $R9 "" +1 +2
  StrCpy $R9 0

  IntOp $R0 $R8 | $R9
  ${If} $R0 <> 0
    MessageBox MB_YESNO|MB_ICONQUESTION "Keep ${ShortName}'s settings in registry?" /SD IDYES IDYES keep_settings
    ${If} $R8 <> 0
      DeleteRegKey HKCU "Software\GNU\x264"
    ${EndIf}
    ${If} $R9 <> 0
      ${If} ${RunningX64}
        SetRegView 64
        DeleteRegKey HKCU "Software\GNU\x264vfw64"
        SetRegView lastused
      ${EndIf}
    ${EndIf}
keep_settings:
  ${EndIf}

  ${If} $R8 <> 0
    Delete /REBOOTOK "$SYSDIR_32bit\x264vfw.dll"

    SetShellVarContext all
    Delete /REBOOTOK "$SMPROGRAMS\${ShortName}\Configure ${ShortName}.lnk"
    SetShellVarContext current
    Delete /REBOOTOK "$SMPROGRAMS\${ShortName}\Configure ${ShortName}.lnk"

    Delete /REBOOTOK "$INSTDIR\x264vfw.dll"
    Delete /REBOOTOK "$INSTDIR\x264vfw.ico"

    !insertmacro UnRegCodec "vidc.x264"
  ${EndIf}

  ${If} $R9 <> 0
    ${If} ${RunningX64}
      ${DisableX64FSRedirection}

      Delete /REBOOTOK "$SYSDIR\x264vfw64.dll"

      SetShellVarContext all
      Delete /REBOOTOK "$SMPROGRAMS\${ShortName}\Configure ${ShortName64}.lnk"
      SetShellVarContext current
      Delete /REBOOTOK "$SMPROGRAMS\${ShortName}\Configure ${ShortName64}.lnk"

      ${EnableX64FSRedirection}

      Delete /REBOOTOK "$INSTDIR\x264vfw64.dll"
      Delete /REBOOTOK "$INSTDIR\x264vfw64.ico"

      SetRegView 64
      !insertmacro UnRegCodec "vidc.x264"
      SetRegView lastused
    ${EndIf}
  ${EndIf}

  SetShellVarContext all
  Delete /REBOOTOK "$SMPROGRAMS\${ShortName}\Uninstall ${ShortName}.lnk"
  RMDir  /REBOOTOK "$SMPROGRAMS\${ShortName}"
  SetShellVarContext current
  Delete /REBOOTOK "$SMPROGRAMS\${ShortName}\Uninstall ${ShortName}.lnk"
  RMDir  /REBOOTOK "$SMPROGRAMS\${ShortName}"

  Delete /REBOOTOK "$INSTDIR\${ShortName}-uninstall.exe"
  RMDir  /REBOOTOK $INSTDIR
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${ShortName}"

SectionEnd

;--------------------------------
;Uninstaller Functions

Function un.onInit

  ${If} ${RunningX64}
    StrCpy $SYSDIR_32bit "$WINDIR\SysWOW64"
  ${Else}
    StrCpy $SYSDIR_32bit $SYSDIR
  ${EndIf}

FunctionEnd
