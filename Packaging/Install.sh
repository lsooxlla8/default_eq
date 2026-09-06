#!/bin/sh

set -eu

package_directory=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
vst3_source="$package_directory/default_eq.vst3"
standalone_source="$package_directory/default_eq-Standalone"
vst3_directory="$HOME/.vst3"
standalone_directory="$HOME/.local/bin"

if [ -d "$vst3_source" ]; then
    mkdir -p "$vst3_directory"
    rm -rf "$vst3_directory/default_eq.vst3"
    cp -R "$vst3_source" "$vst3_directory/default_eq.vst3"
    echo "Installed: $vst3_directory/default_eq.vst3"
fi

if [ -f "$standalone_source" ]; then
    mkdir -p "$standalone_directory"
    cp "$standalone_source" "$standalone_directory/default_eq"
    chmod 755 "$standalone_directory/default_eq"
    echo "Installed: $standalone_directory/default_eq"
fi

echo
echo "default_eq installation complete. Restart your DAW before scanning plug-ins."
printf "Press Return to close..."
read -r _
