!ifndef DLAB_LEGACY_VST3_INCLUDED
!define DLAB_LEGACY_VST3_INCLUDED

Var LegacyFound
Var LegacyList
Var LegacyPath

!macro FindLegacy Name
 StrCpy $LegacyPath "$Vst3Dir\${Name}.vst3"
 IfFileExists "$LegacyPath" 0 +3
  StrCpy $LegacyFound 1
  StrCpy $LegacyList "$LegacyList$\r$\n$LegacyPath"
!macroend

Function DetectLegacyVst3
 StrCpy $LegacyFound 0
 StrCpy $LegacyList ""
 !insertmacro FindLegacy Channel286
 !insertmacro FindLegacy Channel376
 !insertmacro FindLegacy Channel386
 !insertmacro FindLegacy Channel676
FunctionEnd

!macro RemoveLegacy Name
 StrCpy $LegacyPath "$Vst3Dir\${Name}.vst3"
 IfFileExists "$LegacyPath" 0 legacy_done_${Name}
 ; A legacy VST3 can be a single file or a bundle directory. Only remove
 ; these exact names directly inside the selected, resolved VST3 directory.
 System::Call 'kernel32::GetFileAttributesW(w "$LegacyPath") i .r0'
 IntOp $0 $0 & 0x400
 ${If} $0 != 0
  MessageBox MB_OK|MB_ICONSTOP "Cannot remove a linked beta plug-in:$\r$\n$LegacyPath$\r$\n$\r$\nRemove the link manually, then run Setup again."
  Abort "Legacy VST3 is a reparse point."
 ${EndIf}
 ClearErrors
 RMDir /r "$LegacyPath"
 IfFileExists "$LegacyPath" 0 legacy_done_${Name}
 ClearErrors
 Delete "$LegacyPath"
 IfFileExists "$LegacyPath" 0 legacy_done_${Name}
 MessageBox MB_OK|MB_ICONSTOP "Cannot remove the beta plug-in:$\r$\n$LegacyPath$\r$\n$\r$\nClose your DAW and other plug-in hosts, then run Setup again."
 Abort "Legacy VST3 removal failed."
 legacy_done_${Name}:
!macroend

Function RemoveLegacyVst3
 !insertmacro RemoveLegacy Channel286
 !insertmacro RemoveLegacy Channel376
 !insertmacro RemoveLegacy Channel386
 !insertmacro RemoveLegacy Channel676
FunctionEnd
!endif
