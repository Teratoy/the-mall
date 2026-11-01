#!/bin/bash
# Opens the sound editor in the browser, starting it first if it isn't running.
# Builds build/sound_sheet in the toolbox first: the editor renders every sound
# with it, the game's own synth. It keeps running in the background until
# logout; `pkill -f sound_editor.py` stops it.
cd "$(dirname "$(readlink -f "$0")")"
PORT=8767
toolbox run -c claude-build make -s build/sound_sheet
if ! curl -s -o /dev/null "http://localhost:$PORT/api/version"; then
    nohup python3 tools/sound_editor.py "$PORT" >/dev/null 2>&1 &
    for _ in $(seq 50); do
        curl -s -o /dev/null "http://localhost:$PORT/api/version" && break
        sleep 0.1
    done
fi
exec xdg-open "http://localhost:$PORT"
