Unicode true
!include "LogicLib.nsh"
!include "Sections.nsh"
!include "x64.nsh"
!include "nsDialogs.nsh"
!include "WinMessages.nsh"
!include "FileFunc.nsh"
!ifndef PACKAGE_DIR
!error "Pass /DPACKAGE_DIR=<built package>"
!endif
!ifndef UI_FILE
!error "Pass /DUI_FILE=<built dLabInstallerUI.dll>"
!endif
!ifndef OUTPUT_FILE
!define OUTPUT_FILE "dLab.Series-Setup.exe"
!endif
Icon "assets/dLab.ico"
UninstallIcon "assets/dLab.ico"
Name "dLab.Series"
Caption "dLab plugins installer"
OutFile "${OUTPUT_FILE}"
InstallDir "$PROGRAMFILES64\dLab.Series"
!ifdef DLAB_PREVIEW
RequestExecutionLevel user
!else
RequestExecutionLevel admin
!endif
XPStyle on
ManifestDPIAware true
SetCompressor /SOLID lzma
ChangeUI IDD_INST "${UI_FILE}"
ChangeUI IDD_INSTFILES "${UI_FILE}"
ChangeUI IDD_UNINST "${UI_FILE}"
LoadLanguageFile "${NSISDIR}\Contrib\Language files\English.nlf"
BrandingText "Frozz"
ShowInstDetails nevershow
Var Vst2Dir
Var Vst3Dir
Var UseVst2
Var UseVst3
Var Dialog
Var Check2
Var Check3
Var Path2
Var Path3
Var Browse2
Var Browse3
Var TitleFont
Var BodyFont
Var SmallFont
Var HeaderBitmap
!include "LegacyVst3.nsh"
Page custom FormatsCreate FormatsLeave
Page custom PathsCreate PathsLeave
Page instfiles
Page custom FinishCreate
UninstPage uninstConfirm
UninstPage instfiles
!macro Label Y Height Text Font Colour
 ${NSD_CreateLabel} 0 ${Y} 100% ${Height} "${Text}"
 Pop $0
 SendMessage $0 ${WM_SETFONT} ${Font} 1
 SetCtlColors $0 ${Colour} F7F8FA
!macroend

; Custom dialogs need explicit window icons as well as the EXE icon resource.
!macro SetWindowIcons
 InitPluginsDir
 File /oname=$PLUGINSDIR\dLab.ico "${__FILEDIR__}\assets\dLab.ico"
 System::Call 'user32::LoadImageW(p 0, w "$PLUGINSDIR\dLab.ico", i 1, i 16, i 16, i 16) p .r0'
 SendMessage $HWNDPARENT ${WM_SETICON} 0 $0
 System::Call 'user32::LoadImageW(p 0, w "$PLUGINSDIR\dLab.ico", i 1, i 32, i 32, i 16) p .r0'
 SendMessage $HWNDPARENT ${WM_SETICON} 1 $0
!macroend

Section "VST3 - dLab.Series (286 + 376)" SecVst3
!ifndef DLAB_PREVIEW
 Call RemoveLegacyVst3
 SetOutPath "$Vst3Dir\dLab.Series.vst3"
 File /r "${PACKAGE_DIR}\VST3\dLab.Series.vst3\*"
!endif
SectionEnd
Section "VST2 - separate effects for OBS and legacy hosts" SecVst2
!ifndef DLAB_PREVIEW
 SetOutPath "$Vst2Dir"
 File "${PACKAGE_DIR}\VST2\dLab 286.dll"
 File "${PACKAGE_DIR}\VST2\dLab 376.dll"
!endif
SectionEnd
Section -Metadata
!ifndef DLAB_PREVIEW
 SetOutPath "$INSTDIR"
 File /oname=README.md "..\..\..\README.md"
 File /oname=NOTICE.md "..\..\..\NOTICE.md"
 File /oname=Font-OFL.txt "..\..\..\resources\fonts\Channel286\OFL.txt"
 File /oname=VeSTige-COPYING.txt "..\..\..\deps\obs-vst-abi\COPYING"
 File /oname=JUCE-LICENSE.md "..\..\..\deps\JUCE\LICENSE.md"
 File /oname=dLab.ico "${__FILEDIR__}\assets\dLab.ico"
 File /oname=LICENSE "..\..\..\LICENSE"
 WriteUninstaller "$INSTDIR\Uninstall.exe"
 WriteRegStr HKLM "Software\Frozz\dLab.Series" "VST2Dir" "$Vst2Dir"
 WriteRegStr HKLM "Software\Frozz\dLab.Series" "VST3Dir" "$Vst3Dir"
 WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\dLab.Series" "DisplayName" "dLab.Series"
 WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\dLab.Series" "DisplayVersion" "1.0.0"
 WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\dLab.Series" "Publisher" "Frozz"
 WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\dLab.Series" "DisplayIcon" '"$INSTDIR\dLab.ico"'
 WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\dLab.Series" "UninstallString" '"$INSTDIR\Uninstall.exe"'
 WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\dLab.Series" "NoModify" 1
 WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\dLab.Series" "NoRepair" 1
!endif
SectionEnd


Function .onInit
 System::Call 'kernel32::LoadLibraryW(w "riched20.dll") p .r0'
 ${IfNot} ${RunningX64}
  MessageBox MB_OK "This build requires 64-bit Windows."
  Quit
 ${EndIf}
 SetRegView 64
 StrCpy $UseVst2 1
 StrCpy $UseVst3 1
 StrCpy $Vst2Dir "$COMMONFILES64\VST2\dLab.Series"
 StrCpy $Vst3Dir "$COMMONFILES64\VST3"
 ReadRegStr $0 HKLM "Software\Frozz\dLab.Series" "VST2Dir"
 ${If} $0 != ""
  StrCpy $Vst2Dir $0
 ${EndIf}
 ReadRegStr $0 HKLM "Software\Frozz\dLab.Series" "VST3Dir"
 ${If} $0 != ""
  StrCpy $Vst3Dir $0
 ${EndIf}
FunctionEnd
!macro StyleInstallerWindow
 !insertmacro SetWindowIcons
 CreateFont $TitleFont "Segoe UI" 17 600
 CreateFont $BodyFont "Segoe UI" 11 400
 CreateFont $SmallFont "Segoe UI" 10 400
 SetCtlColors $HWNDPARENT 20252D F7F8FA
 SendMessage $HWNDPARENT ${WM_SETTEXT} 0 "STR:dLab plugins installer"
 InitPluginsDir
 File /oname=$PLUGINSDIR\header.bmp "${__FILEDIR__}\assets\dLab-header.bmp"
 System::Call 'user32::LoadImageW(p 0, w "$PLUGINSDIR\header.bmp", i 0, i 190, i 64, i 16) p .r0'
 StrCpy $HeaderBitmap $0
 GetDlgItem $0 $HWNDPARENT 1037
 SendMessage $0 ${STM_SETIMAGE} 0 $HeaderBitmap
 GetDlgItem $0 $HWNDPARENT 1038
 SendMessage $0 ${WM_SETTEXT} 0 "STR:Open-Source Audio Tools"
 SendMessage $0 ${WM_SETFONT} $SmallFont 1
 SetCtlColors $0 69717D F7F8FA
 GetDlgItem $0 $HWNDPARENT 1
 SendMessage $0 ${WM_SETFONT} $SmallFont 1
 GetDlgItem $0 $HWNDPARENT 2
 SendMessage $0 ${WM_SETFONT} $SmallFont 1
 GetDlgItem $0 $HWNDPARENT 3
 SendMessage $0 ${WM_SETFONT} $SmallFont 1
 GetDlgItem $0 $HWNDPARENT 1028
 SendMessage $0 ${WM_SETFONT} $SmallFont 1
 SendMessage $0 ${WM_SETTEXT} 0 "STR:Software is like $$ex: It's better when it's free."
 SendMessage $0 1091 0 0xFAF8F7 ; EM_SETBKGNDCOLOR, COLORREF
!macroend
Function .onGUIInit
 !insertmacro StyleInstallerWindow
FunctionEnd
Function un.onInit
 System::Call 'kernel32::LoadLibraryW(w "riched20.dll") p .r0'
FunctionEnd
Function FormatsCreate
 GetDlgItem $0 $HWNDPARENT 1
 SendMessage $0 ${WM_SETTEXT} 0 "STR:Continue"
 nsDialogs::Create 1018
 Pop $Dialog
 SetCtlColors $Dialog 20252D F7F8FA
 !insertmacro Label 0u 28u "Choose plug-in formats" $TitleFont 20252D
 !insertmacro Label 32u 25u "Install one format or both. Each includes dLab 286 and dLab 376." $SmallFont 69717D
 ${NSD_CreateCheckbox} 0 68u 100% 22u "VST3  /  DAWs"
 Pop $Check3
 SendMessage $Check3 ${WM_SETFONT} $BodyFont 1
 SetCtlColors $Check3 20252D F7F8FA
 ${If} $UseVst3 == 1
  ${NSD_Check} $Check3
 ${EndIf}
 !insertmacro Label 94u 25u "One dLab.Series module, two independent effects." $SmallFont 69717D
 ${NSD_CreateCheckbox} 0 133u 100% 22u "VST2  /  OBS and legacy hosts"
 Pop $Check2
 SendMessage $Check2 ${WM_SETFONT} $BodyFont 1
 SetCtlColors $Check2 20252D F7F8FA
 ${If} $UseVst2 == 1
  ${NSD_Check} $Check2
 ${EndIf}
 !insertmacro Label 159u 25u "Separate dLab 286.dll and dLab 376.dll for broad compatibility." $SmallFont 69717D
 nsDialogs::Show
FunctionEnd
Function FormatsLeave
 ${NSD_GetState} $Check3 $UseVst3
 ${NSD_GetState} $Check2 $UseVst2
 ${If} $UseVst3 == 0
 ${AndIf} $UseVst2 == 0
  MessageBox MB_OK "Select at least one plug-in format."
  Abort
 ${EndIf}
 SectionSetFlags ${SecVst3} $UseVst3
 SectionSetFlags ${SecVst2} $UseVst2
FunctionEnd
Function PathsCreate
 GetDlgItem $0 $HWNDPARENT 3
 SendMessage $0 ${WM_SETTEXT} 0 "STR:Back"
 nsDialogs::Create 1018
 Pop $Dialog
 SetCtlColors $Dialog 20252D F7F8FA
 !insertmacro Label 0u 28u "Choose installation folders" $TitleFont 20252D
 !insertmacro Label 32u 25u "Use folders scanned by your DAW or OBS." $SmallFont 69717D
 !insertmacro Label 62u 18u "VST3" $BodyFont 20252D
 ${NSD_CreateText} 0 84u 79% 22u "$Vst3Dir"
 Pop $Path3
 SendMessage $Path3 ${WM_SETFONT} $SmallFont 1
 ${NSD_CreateBrowseButton} 81% 84u 19% 22u "Browse"
 Pop $Browse3
 SendMessage $Browse3 ${WM_SETFONT} $SmallFont 1
 ${NSD_OnClick} $Browse3 BrowseVst3
 !insertmacro Label 113u 25u "VST2" $BodyFont 20252D
 ${NSD_CreateText} 0 142u 79% 22u "$Vst2Dir"
 Pop $Path2
 SendMessage $Path2 ${WM_SETFONT} $SmallFont 1
 ${NSD_CreateBrowseButton} 81% 142u 19% 22u "Browse"
 Pop $Browse2
 SendMessage $Browse2 ${WM_SETFONT} $SmallFont 1
 ${NSD_OnClick} $Browse2 BrowseVst2
 ${If} $UseVst3 == 0
  EnableWindow $Path3 0
  EnableWindow $Browse3 0
 ${EndIf}
 ${If} $UseVst2 == 0
  EnableWindow $Path2 0
  EnableWindow $Browse2 0
 ${EndIf}
 nsDialogs::Show
FunctionEnd
Function BrowseVst3
 Pop $0
 ${NSD_GetText} $Path3 $1
 nsDialogs::SelectFolderDialog "VST3 installation folder" $1
 Pop $1
 ${If} $1 != "error"
  ${NSD_SetText} $Path3 $1
 ${EndIf}
FunctionEnd
Function BrowseVst2
 Pop $0
 ${NSD_GetText} $Path2 $1
 nsDialogs::SelectFolderDialog "VST2 installation folder" $1
 Pop $1
 ${If} $1 != "error"
  ${NSD_SetText} $Path2 $1
 ${EndIf}
FunctionEnd
Function PathsLeave
 ${NSD_GetText} $Path3 $Vst3Dir
 ${NSD_GetText} $Path2 $Vst2Dir
 ${If} $UseVst3 == 1
 ${AndIf} $Vst3Dir == ""
  MessageBox MB_OK "Choose a VST3 folder."
  Abort
 ${EndIf}
 ${If} $UseVst2 == 1
 ${AndIf} $Vst2Dir == ""
  MessageBox MB_OK "Choose a VST2 folder."
  Abort
 ${EndIf}
 ${If} $UseVst3 == 1
  GetFullPathName $Vst3Dir "$Vst3Dir"
  Call DetectLegacyVst3
  ${If} $LegacyFound == 1
   MessageBox MB_OKCANCEL|MB_ICONEXCLAMATION "Beta plug-ins found:$LegacyList$\r$\n$\r$\nThese will be removed when installation starts to avoid duplicate effects. Close your DAW and other plug-in hosts before continuing.$\r$\n$\r$\nClick OK to continue or Cancel to return to the folder selection." IDOK legacy_approved
   Abort
   legacy_approved:
  ${EndIf}
 ${EndIf}
FunctionEnd
Function FinishCreate
 nsDialogs::Create 1018
 Pop $Dialog
 SetCtlColors $Dialog 20252D F7F8FA
 !insertmacro Label 0u 32u "Ready to make sound" $TitleFont 20252D
 !insertmacro Label 42u 30u "dLab.Series has been installed. Rescan plug-ins in your host to find the effects." $BodyFont 20252D
 ${If} $UseVst3 == 1
  !insertmacro Label 93u 40u "VST3: $Vst3Dir" $SmallFont 69717D
 ${EndIf}
 ${If} $UseVst2 == 1
  !insertmacro Label 139u 40u "VST2: $Vst2Dir" $SmallFont 69717D
 ${EndIf}
 nsDialogs::Show
FunctionEnd
Function un.onGUIInit
 !insertmacro StyleInstallerWindow
FunctionEnd
Section "Uninstall"
!ifndef DLAB_PREVIEW
 SetRegView 64
 ReadRegStr $Vst2Dir HKLM "Software\Frozz\dLab.Series" "VST2Dir"
 ReadRegStr $Vst3Dir HKLM "Software\Frozz\dLab.Series" "VST3Dir"
 ${If} $Vst3Dir != ""
  RMDir /r "$Vst3Dir\dLab.Series.vst3"
 ${EndIf}
 ${If} $Vst2Dir != ""
  Delete "$Vst2Dir\dLab 286.dll"
  Delete "$Vst2Dir\dLab 376.dll"
  RMDir "$Vst2Dir"
 ${EndIf}
 Delete "$INSTDIR\README.md"
 Delete "$INSTDIR\NOTICE.md"
 Delete "$INSTDIR\LICENSE"
 Delete "$INSTDIR\Font-OFL.txt"
 Delete "$INSTDIR\VeSTige-COPYING.txt"
 Delete "$INSTDIR\JUCE-LICENSE.md"
 Delete "$INSTDIR\dLab.ico"
 Delete "$INSTDIR\Uninstall.exe"
 RMDir "$INSTDIR"
 DeleteRegKey HKLM "Software\Frozz\dLab.Series"
 DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\dLab.Series"
!endif
SectionEnd
