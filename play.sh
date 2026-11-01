#!/bin/bash
# Rebuilds if needed inside the toolbox that has raylib, then starts The Mall
# on the host, where the sound libraries are (the toolbox has none, so the
# game would be silent there). The game finds raylib in build/lib.
cd "$(dirname "$(readlink -f "$0")")"
toolbox run -c claude-build make -s -j8 all || exit 1
exec build/themall "$@"
