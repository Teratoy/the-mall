#!/bin/bash
# Opens the sprite editor in the browser, starting it first if it isn't running.
# It keeps running in the background until logout; `pkill -f sprite_editor.py` stops it.
cd "$(dirname "$(readlink -f "$0")")"
PORT=8766
if ! curl -s -o /dev/null "http://localhost:$PORT/api/version"; then
    nohup python3 tools/sprite_editor.py "$PORT" >/dev/null 2>&1 &
    for _ in $(seq 50); do
        curl -s -o /dev/null "http://localhost:$PORT/api/version" && break
        sleep 0.1
    done
fi
exec xdg-open "http://localhost:$PORT"
