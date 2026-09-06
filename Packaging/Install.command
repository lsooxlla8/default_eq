#!/bin/bash

set -e

package_directory="$(cd "$(dirname "$0")" && pwd)"
vst3_source="$package_directory/default_eq.vst3"
au_source="$package_directory/default_eq.component"
app_source="$package_directory/default_eq.app"
vst3_directory="$HOME/Library/Audio/Plug-Ins/VST3"
au_directory="$HOME/Library/Audio/Plug-Ins/Components"
app_directory="$HOME/Applications"

install_bundle() {
    local source_path="$1"
    local destination_directory="$2"
    if [ ! -e "$source_path" ]; then
        return
    fi
    mkdir -p "$destination_directory"
    local destination_path="$destination_directory/$(basename "$source_path")"
    rm -rf "$destination_path"
    /usr/bin/ditto "$source_path" "$destination_path"
    /usr/bin/xattr -dr com.apple.quarantine "$destination_path" 2>/dev/null || true
    echo "Installed: $destination_path"
}

install_bundle "$vst3_source" "$vst3_directory"
install_bundle "$au_source" "$au_directory"
install_bundle "$app_source" "$app_directory"

killall -9 AudioComponentRegistrar 2>/dev/null || true

echo
echo "default_eq installation complete. Restart your DAW before scanning plug-ins."
read -r -p "Press Return to close..." _
