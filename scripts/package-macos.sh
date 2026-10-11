#!/bin/bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
build_dir="${1:-$repo_root/build/macos-universal}"
output="${2:-$build_dir/dLab.Series-macOS-Universal.pkg}"
version="${3:-1.0.0}"
build_dir="$(cd "$build_dir" && pwd)"
payload="$build_dir/Package/dLab.Series"
stage="$(mktemp -d "$build_dir/package-macos.XXXXXX")"
mkdir -p "$stage/packages" "$stage/resources"
for format in VST3 AU VST2; do
  test -d "$payload/$format" || { echo "Missing $format payload. Build dLab_Series_Package with VST2 enabled first." >&2; exit 1; }
done

# Prevent Installer from relocating an older beta bundle in another folder.
for format in VST3 AU VST2; do
  python3 - "$payload/$format" "$stage/$format.plist" <<'PY'
import pathlib, plistlib, sys
entries = [{"RootRelativeBundlePath": p.name, "BundleIsRelocatable": False,
            "BundleIsVersionChecked": False, "BundleOverwriteAction": "upgrade"}
           for p in sorted(pathlib.Path(sys.argv[1]).iterdir()) if p.is_dir()]
with open(sys.argv[2], "wb") as out: plistlib.dump(entries, out)
PY
  case "$format" in
    VST3) location=/Library/Audio/Plug-Ins/VST3; identifier=vst3 ;;
    AU) location=/Library/Audio/Plug-Ins/Components; identifier=au ;;
    VST2) location=/Library/Audio/Plug-Ins/VST; identifier=vst2 ;;
  esac
  pkgbuild --root "$payload/$format" --component-plist "$stage/$format.plist" \
    --identifier "com.frozz.dlab.series.$identifier" --version "$version" \
    --install-location "$location" "$stage/packages/$format.pkg"
done
cp "$repo_root/resources/macos/Welcome.html" "$stage/resources/Welcome.html"
cp "$repo_root/resources/images/dLab-logo.png" "$stage/resources/dLab-logo.png"
python3 - "$repo_root" "$stage/resources/License.html" <<'PY'
import html, pathlib, sys
root = pathlib.Path(sys.argv[1])
sections = [("dLab.Series original code — MIT", root/"LICENSE"),
            ("Dependency notices", root/"NOTICE.md"),
            ("JUCE", root/"deps/JUCE/LICENSE.md"),
            ("VeSTige VST2 ABI", root/"deps/obs-vst-abi/COPYING"),
            ("Barlow fonts", root/"resources/fonts/Channel286/OFL.txt")]
content = '<html><body style="font-family: -apple-system; font-size: 13px">'
for title, path in sections:
    content += '<h2>'+html.escape(title)+'</h2><pre style="white-space:pre-wrap">'+html.escape(path.read_text())+'</pre>'
pathlib.Path(sys.argv[2]).write_text(content+'</body></html>')
PY
cat > "$stage/Distribution.xml" <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
 <title>dLab plugins installer</title>
 <welcome file="Welcome.html" mime-type="text/html"/>
 <license file="License.html" mime-type="text/html"/>
 <options customize="always" require-scripts="false" hostArchitectures="x86_64,arm64"/>
 <domains enable_localSystem="true" enable_currentUserHome="false" enable_anywhere="false"/>
 <allowed-os-versions><os-version min="11.0"/></allowed-os-versions>
 <choices-outline><line choice="formats"><line choice="vst3"/><line choice="au"/><line choice="vst2"/></line></choices-outline>
 <choice id="formats" title="dLab audio plug-ins"/>
 <choice id="vst3" title="VST3 — dLab 286 and dLab 376" start_selected="true" description="One module containing two independent effects."><pkg-ref id="com.frozz.dlab.series.vst3"/></choice>
 <choice id="au" title="Audio Units — dLab 286 and dLab 376" start_selected="true" description="One component registering two independent effects."><pkg-ref id="com.frozz.dlab.series.au"/></choice>
 <choice id="vst2" title="VST2 — separate effects for legacy hosts" start_selected="false" description="Two ordinary VST2 plug-ins; no shell."><pkg-ref id="com.frozz.dlab.series.vst2"/></choice>
 <pkg-ref id="com.frozz.dlab.series.vst3" version="$version">VST3.pkg</pkg-ref>
 <pkg-ref id="com.frozz.dlab.series.au" version="$version">AU.pkg</pkg-ref>
 <pkg-ref id="com.frozz.dlab.series.vst2" version="$version">VST2.pkg</pkg-ref>
</installer-gui-script>
XML
productbuild --distribution "$stage/Distribution.xml" --resources "$stage/resources" \
  --package-path "$stage/packages" "$output"
pkgutil --check-signature "$output" || true # Development installer is unsigned.
echo "Built $output (unsigned development package; no installation performed)."
