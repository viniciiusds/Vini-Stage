#!/bin/bash
set -euo pipefail
umask 022
project_dir="$(cd "$(dirname "$0")" && pwd)"
build_dir="${1:-$project_dir/build}"
output_dir="${2:-$project_dir/dist}"
bundle="$build_dir/NpnoPlayer_artefacts/Release/VST3/Vini Stage.vst3"
app_bundle="$build_dir/NpnoPlayer_artefacts/Release/Standalone/Vini Stage.app"
for candidate in "$bundle" "$app_bundle"; do
    [[ -f "$candidate/Contents/MacOS/Vini Stage" ]] || { echo "Compile VST3 e Standalone antes de empacotar: $candidate" >&2;exit 1; }
done
version="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "$bundle/Contents/Info.plist")"
[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo "Versão inválida." >&2;exit 1; }
/usr/bin/lipo "$bundle/Contents/MacOS/Vini Stage" -verify_arch x86_64 arm64
/usr/bin/codesign --verify --deep --strict "$bundle"
mkdir -p "$output_dir"
output_dir="$(cd "$output_dir" && pwd)"
stage="$(mktemp -d "$build_dir/pkg-stage.XXXXXX")"
trap 'pkg_exit=$?; rm -rf "$stage"; exit "$pkg_exit"' EXIT
vst_root="$stage/vst-root"
app_root="$stage/app-root"
shared_root="$stage/shared-root"
mkdir -p "$vst_root/Library/Audio/Plug-Ins/VST3" "$app_root/Applications"     "$shared_root/Library/Application Support/Vini Stage/Licenses" "$stage/packages"
/usr/bin/ditto "$bundle" "$vst_root/Library/Audio/Plug-Ins/VST3/Vini Stage.vst3"
/usr/bin/ditto "$app_bundle" "$app_root/Applications/Vini Stage.app"
app_version="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "$app_bundle/Contents/Info.plist")"
[[ "$app_version" == "$version" ]] || { echo "VST3 e Standalone têm versões diferentes." >&2;exit 1; }
/usr/bin/lipo "$app_bundle/Contents/MacOS/Vini Stage" -verify_arch x86_64 arm64
if ! /usr/bin/codesign --verify --deep --strict "$app_root/Applications/Vini Stage.app" 2>/dev/null; then
    /usr/bin/codesign --force --deep --sign - "$app_root/Applications/Vini Stage.app"
fi
/usr/bin/codesign --verify --deep --strict "$app_root/Applications/Vini Stage.app"
cp "$project_dir/LICENSE" "$shared_root/Library/Application Support/Vini Stage/Licenses/AGPL-3.0.txt"
cp "$project_dir/Licenses/"* "$shared_root/Library/Application Support/Vini Stage/Licenses/"
cp "$project_dir/docs/USO.md" "$shared_root/Library/Application Support/Vini Stage/LEIA-ME.md"
printf '%s\n' 'Código-fonte: https://github.com/viniciiusds/Vini-Stage' > "$shared_root/Library/Application Support/Vini Stage/SOURCE.txt"
# Fixed destinations and independent receipts allow either component to update
# without moving a bundle or touching the component the user did not select.
bundle_metadata(){
    local relative_path="$1"
    local output="$2"
    cat > "$output" <<PLIST
<?xml version="1.0" encoding="UTF-8"?><!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><array><dict>
<key>RootRelativeBundlePath</key><string>$relative_path</string>
<key>BundleIsRelocatable</key><false/>
<key>BundleIsVersionChecked</key><true/>
<key>BundleHasStrictIdentifier</key><true/>
<key>BundleOverwriteAction</key><string>upgrade</string>
</dict></array></plist>
PLIST
}
bundle_metadata 'Library/Audio/Plug-Ins/VST3/Vini Stage.vst3' "$stage/vst-components.plist"
bundle_metadata 'Applications/Vini Stage.app' "$stage/app-components.plist"
/usr/bin/pkgbuild --root "$vst_root" --component-plist "$stage/vst-components.plist"     --scripts "$project_dir/packaging/scripts" --identifier audio.vinizera.vinistage.vst3     --version "$version" --install-location / --ownership recommended --compression legacy     "$stage/packages/ViniStageComponent.pkg"
/usr/bin/pkgbuild --root "$app_root" --component-plist "$stage/app-components.plist"     --identifier audio.vinizera.vinistage.standalone --version "$version" --install-location /     --ownership recommended --compression legacy "$stage/packages/ViniStageStandalone.pkg"
/usr/bin/pkgbuild --root "$shared_root" --identifier audio.vinizera.vinistage.resources     --version "$version" --install-location / --ownership recommended --compression legacy     "$stage/packages/ViniStageResources.pkg"
cat > "$stage/Distribution.xml" <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="1">
<title>Vini Stage $version</title>
<organization>audio.vinizera</organization>
<welcome file="welcome.html" mime-type="text/html"/>
<conclusion file="conclusion.html" mime-type="text/html"/>
<options customize="always" require-scripts="true" hostArchitectures="x86_64,arm64" rootVolumeOnly="true"/>
<domains enable_anywhere="false" enable_currentUserHome="false" enable_localSystem="true"/>
<volume-check><allowed-os-versions><os-version min="10.13"/></allowed-os-versions></volume-check>
<choices-outline><line choice="vst3"/><line choice="standalone"/><line choice="resources"/></choices-outline>
<choice id="vst3" title="VST3" description="Instala ou atualiza /Library/Audio/Plug-Ins/VST3/Vini Stage.vst3" visible="true" enabled="true" start_selected="true"><pkg-ref id="audio.vinizera.vinistage.vst3"/></choice>
<choice id="standalone" title="Standalone" description="Instala ou atualiza /Applications/Vini Stage.app" visible="true" enabled="true" start_selected="true"><pkg-ref id="audio.vinizera.vinistage.standalone"/></choice>
<choice id="resources" title="Documentação e licenças" visible="false" enabled="false" selected="choices.vst3.selected || choices.standalone.selected"><pkg-ref id="audio.vinizera.vinistage.resources"/></choice>
<pkg-ref id="audio.vinizera.vinistage.vst3" version="$version" onConclusion="none">ViniStageComponent.pkg</pkg-ref>
<pkg-ref id="audio.vinizera.vinistage.standalone" version="$version" onConclusion="none">ViniStageStandalone.pkg</pkg-ref>
<pkg-ref id="audio.vinizera.vinistage.resources" version="$version" onConclusion="none">ViniStageResources.pkg</pkg-ref>
</installer-gui-script>
XML
package="$output_dir/Vini-Stage-$version-Universal.pkg"
build_product(){
    /usr/bin/productbuild --distribution "$stage/Distribution.xml" --resources "$project_dir/packaging/resources" \
        --package-path "$stage/packages" "$@" "$package"
}
if [[ -n "${INSTALLER_SIGN_IDENTITY:-}" ]]; then build_product --sign "$INSTALLER_SIGN_IDENTITY";else build_product;fi
echo "Criado: $package"
if [[ -z "${INSTALLER_SIGN_IDENTITY:-}" ]]; then echo "Pacote sem assinatura Developer ID Installer e sem notarização.";fi
