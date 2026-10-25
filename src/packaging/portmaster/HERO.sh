#!/bin/bash
# PortMaster launcher for the native H.E.R.O. (C64 version, SDL2).

XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}

if [ -d "/opt/system/Tools/PortMaster/" ]; then
  controlfolder="/opt/system/Tools/PortMaster"
elif [ -d "/opt/tools/PortMaster/" ]; then
  controlfolder="/opt/tools/PortMaster"
elif [ -d "$XDG_DATA_HOME/PortMaster/" ]; then
  controlfolder="$XDG_DATA_HOME/PortMaster"
else
  controlfolder="/roms/ports/PortMaster"
fi

source $controlfolder/control.txt

[ -f "${controlfolder}/mod_${CFW_NAME}.txt" ] && source "${controlfolder}/mod_${CFW_NAME}.txt"
get_controls

GAMEDIR=/$directory/ports/hero

> "$GAMEDIR/log.txt" && exec > >(tee "$GAMEDIR/log.txt") 2>&1

cd $GAMEDIR

# the player's own C64 tape image sits beside the program
for tap in "$GAMEDIR/H.E.R.O.tap" "$GAMEDIR/hero.tap" "$GAMEDIR/HERO.tap"; do
  [ -e "$tap" ] && export HERO_TAP="$tap" && break
done

# gptokeyb only provides the exit hotkey; the game reads the pad itself
$GPTOKEYB "hero.${DEVICE_ARCH}" &

SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig" ./hero.${DEVICE_ARCH}

pm_finish
