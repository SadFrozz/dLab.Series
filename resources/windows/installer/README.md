# dLab.Series NSIS installer

Windows x64 only. The custom format-selection page offers VST3, VST2 or both. The next page provides separate editable folders and Browse buttons for both formats; unselected formats have disabled fields. AU is packaged for macOS and is not offered on Windows.

Build `dlab_installer_ui` and `dLab_Series_Package`, then run `scripts/package-windows.ps1` with NSIS 3. The resource DLL supplies the dialog, logo, footer and progress layout. Text uses native Segoe UI. Both the window and Windows Apps entry use the dLab icon.

`DLAB_PREVIEW=1` produces an interface-only preview build with all installation and uninstallation actions omitted. This is for visual verification and is not a distributable installer.
