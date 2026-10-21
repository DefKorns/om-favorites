#!/bin/sh
# shellcheck disable=SC2086,SC2154,SC3046,SC3051
#  Copyright (c) 2026 DefKorns (https://defkorns.github.io/LICENSE)
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <https://www.gnu.org/licenses/>.
#
# favorites_manager exit: 0 = back to opening menu, 2 = favorites changed, else leave
#
# shellcheck source=/dev/null
options_path="${1%/}"
[ -z "$options_path" ] && options_path="/etc/options_menu"

exec > /tmp/favorites_manager.log 2>&1

echo "=== Favorites Manager Launcher ==="
echo "Date: $(date)"
echo "Options path: $options_path"
echo "OM_BACK_STACK: $OM_BACK_STACK"

games_args=""
if [ -f /etc/preinit ]; then
  source /etc/preinit
  script_init
  storage="$(findGameStorage 2>/dev/null)"
  if [ -n "$storage" ]; then
    games_root="${storage%/000}"
    echo "Resolved games root via findGameStorage: $games_root"
    games_args="--gamesPath $games_root"
  fi
fi

echo "Launching: $options_path/favorites_manager/favorites_manager $games_args $options_path"
"$options_path/favorites_manager/favorites_manager" $games_args "$options_path"
EXIT_CODE=$?
echo "favorites_manager finished with code: $EXIT_CODE"

# chmenu refuses the folder already shown, and uistop would kill this script
# (it inherits options' /dev/fb0): remote-exec runs the reload instead
if [ $EXIT_CODE -eq 2 ]; then
  echo "Favorites changed, reloading the home menu..."
  "$options_path/scripts/gen_splash" /tmp/favorites_splash.png FAVORITES_RELOADING && decodepng /tmp/favorites_splash.png >/dev/fb0
  menu_state="/var/saves/$modname/menu"
  current_menu="000"
  [ -f "$menu_state" ] && current_menu="$(cat "$menu_state")"
  cat > /var/exec.flag.tmp <<EOF
source /etc/preinit
script_init
rm -f /tmp/options.flag
uistop
uiresume
echo reload > "$menu_state"
/bin/chmenu "$current_menu" || /bin/chmenu 000 || { echo "$current_menu" > "$menu_state"; uistart; }
EOF
  mv /var/exec.flag.tmp /var/exec.flag
  exit 0
fi

if [ $EXIT_CODE -ne 0 ]; then
  echo "Non-zero exit, resuming Clover UI..."
  sh "$options_path/scripts/ResumeUI.sh"
  exit 0
fi

entry="${OM_BACK_STACK##*;}"
remaining="${OM_BACK_STACK%;*}"
[ "$remaining" = "$OM_BACK_STACK" ] && remaining=""
command_path="${entry%%,*}"
rest="${entry#*,}"
script_path="${rest%%,*}"
title_rest="${rest#*,}"
title_key="${title_rest%%,*}"
marker=""
[ "$title_rest" != "$title_key" ] && marker="${title_rest#*,}"

usleep 50000
set -- --commandPath "$command_path"
[ -n "$script_path" ] && set -- "$@" --scriptPath "$script_path"
set -- "$@" --title "$title_key"
[ -n "$marker" ] && set -- "$@" --selectedMarker "$marker"
OM_BACK_STACK="$remaining" "$options_path/options" "$@"
