#!/bin/bash
# Move an older/equal, matching per-user bundle outside the VST3 scan folder.
# When Installer calls this as root, all writes run as the owning user.
set -euo pipefail
user_home="${1:?User home required}"
installed="${2:?Installed global bundle required}"
account="${3:-}"
candidate="$user_home/Library/Audio/Plug-Ins/VST3/Vini Stage.vst3"
[[ -d "$candidate" && ! -L "$candidate" ]] || exit 0
plist_value() { /usr/libexec/PlistBuddy -c "Print :$2" "$1/Contents/Info.plist" 2>/dev/null; }
[[ "$(plist_value "$installed" CFBundleIdentifier || true)" == "audio.vd.npnoplayer" ]] || exit 0
[[ "$(plist_value "$candidate" CFBundleIdentifier || true)" == "audio.vd.npnoplayer" ]] || exit 0
global_version="$(plist_value "$installed" CFBundleShortVersionString || true)"
user_version="$(plist_value "$candidate" CFBundleShortVersionString || true)"
# Unknown version strings are left untouched. Never replace a newer user build.
[[ "$global_version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ && "$user_version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || exit 0
if ! /usr/bin/awk -v installed="$global_version" -v current="$user_version" 'BEGIN {
    split(installed,a,".");split(current,b,".");
    for(i=1;i<=3;i++){if(a[i]+0>b[i]+0)exit 0;if(a[i]+0<b[i]+0)exit 1;}exit 0;
}'; then
    echo "Vini Stage: keeping newer user version $user_version."
    exit 0
fi
as_owner() {
    if [[ "$EUID" -eq 0 ]]; then
        [[ -n "$account" && "$account" != root ]] || return 1
        /usr/bin/sudo -u "$account" -- "$@"
    else
        "$@"
    fi
}
backup="$user_home/Library/Application Support/Vini Stage/Plugin Backups/$(date +%Y%m%d-%H%M%S)-$$"
as_owner /bin/mkdir -p "$backup"
as_owner /bin/mv "$candidate" "$backup/Vini Stage.vst3"
echo "Vini Stage: previous user bundle backed up to $backup"
