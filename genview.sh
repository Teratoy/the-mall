#!/bin/bash
# Rebuilds if needed, then opens the world generator editor inside the toolbox that has raylib.
cd "$(dirname "$(readlink -f "$0")")"
exec toolbox run -c claude-build make -s genview
