#!/bin/bash
set -euo pipefail
umask 022
project_dir="$(cd "$(dirname "$0")" && pwd)"
build_dir="${1:-$project_dir/build}"
output_dir="${2:-$project_dir/dist}"
bundle="$build_dir/NpnoPlayer_artefacts/Release/VST3/Vini Stage.vst3"
[[ -f "$bundle/Contents/MacOS/Vini Stage" ]] || { echo "Compile o VST3 antes de empacotar." >&2;exit 1; }
version="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "$bundle/Contents/Info.plist")"
[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo "Versão inválida." >&2;exit 1; }
/usr/bin/lipo "$bundle/Contents/MacOS/Vini Stage" -verify_arch x86_64 arm64
/usr/bin/codesign --verify --deep --strict "$bundle"
mkdir -p "$output_dir"
output_dir="$(cd "$output_dir" && pwd)"
stage="$(mktemp -d "$build_dir/pkg-stage.XXXXXX")"
trap 'pkg_exit=$?; rm -rf "$stage"; exit "$pkg_exit"' EXIT
payload="$stage/root"
mkdir -p "$payload/Library/Audio/Plug-Ins/VST3" "$payload/Library/Application Support/Vini Stage/Licenses" "$stage/packages"
/usr/bin/ditto "$bundle" "$payload/Library/Audio/Plug-Ins/VST3/Vini Stage.vst3"
cp "$project_dir/LICENSE" "$payload/Library/Application Support/Vini Stage/Licenses/AGPL-3.0.txt"
cp "$project_dir/Licenses/"* "$payload/Library/Application Support/Vini Stage/Licenses/"
cp "$project_dir/docs/USO.md" "$payload/Library/Application Support/Vini Stage/LEIA-ME.md"
printf '%s\n' 'Código-fonte: https://github.com/viniciiusds/Vini-Stage' > "$payload/Library/Application Support/Vini Stage/SOURCE.txt"
# Explicit bundle metadata prevents relocation and atomically replaces the old
# bundle on upgrade. Legacy compression preserves compatibility with 10.13.
cat > "$stage/components.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?><!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><array><dict>
<key>RootRelativeBundlePath</key><string>Library/Audio/Plug-Ins/VST3/Vini Stage.vst3</string>
<key>BundleIsRelocatable</key><false/>
<key>BundleIsVersionChecked</key><true/>
<key>BundleHasStrictIdentifier</key><true/>
<key>BundleOverwriteAction</key><string>upgrade</string>
</dict></array></plist>
PLIST
/usr/bin/pkgbuild --root "$payload" --component-plist "$stage/components.plist" \
    --scripts "$project_dir/packaging/scripts" --identifier audio.vinizera.vinistage.vst3 \
    --version "$version" --install-location / --ownership recommended --compression legacy \
    "$stage/packages/ViniStageComponent.pkg"
cat > "$stage/Distribution.xml" <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="1">
<title>Vini Stage $version</title>
<organization>audio.vinizera</organization>
<welcome file="welcome.html" mime-type="text/html"/>
<conclusion file="conclusion.html" mime-type="text/html"/>
<options customize="never" require-scripts="false" hostArchitectures="x86_64,arm64" rootVolumeOnly="true"/>
<domains enable_anywhere="false" enable_currentUserHome="false" enable_localSystem="true"/>
<volume-check><allowed-os-versions><os-version min="10.13"/></allowed-os-versions></volume-check>
<choices-outline><line choice="default"/></choices-outline>
<choice id="default" title="Vini Stage VST3"><pkg-ref id="audio.vinizera.vinistage.vst3"/></choice>
<pkg-ref id="audio.vinizera.vinistage.vst3" version="$version" onConclusion="none">ViniStageComponent.pkg</pkg-ref>
</installer-gui-script>
XML
package="$output_dir/Vini-Stage-$version-Universal.pkg"
build_product(){
    /usr/bin/productbuild --distribution "$stage/Distribution.xml" --resources "$project_dir/packaging/resources" \
        --package-path "$stage/packages" "$@" "$package"
}
if [[ -n "${INSTALLER_SIGN_IDENTITY:-}" ]]; then build_product --sign "$INSTALLER_SIGN_IDENTITY";else build_product;fi
echo "Criado: $package"
if [[ -z "${INSTALLER_SIGN_IDENTITY:-}" ]]; then echo "Pacote de teste sem assinatura Developer ID Installer e sem notarização.";fi
