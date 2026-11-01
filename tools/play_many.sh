#!/bin/bash
# Runs the game in several windows side by side at top speed, a new seed in
# each slot every few minutes, to watch many worlds at once and catch crashes.
#   tools/play_many.sh [slots] [seconds per world] [first seed]
# Each world's humans' life logs go to build/play_many/SEED.txt, the game's
# console output to SEED.log, a picture of how it ended to SEED.png, and
# crashes (an exit before its time is up, or with an error) to crashes.txt.
# Needs wmctrl and xdotool, and the game under X11 or XWayland.
cd "$(dirname "$(readlink -f "$0")")/.."
toolbox run -c claude-build make -s -j8 all || exit 1
SLOTS=${1:-5}
SECONDS_EACH=${2:-300}
SEED=${3:-1000}
OUT=build/play_many
mkdir -p "$OUT"
W=$(( 1920 / SLOTS ))
H=1040

slot() {
    local i=$1 seed=$2
    while :; do
        build/themall --seed "$seed" --speed 5 --logs "$OUT/$seed.txt" --quit-after "$SECONDS_EACH" > "$OUT/$seed.log" 2>&1 &
        local pid=$! win=""
        for _ in $(seq 50); do
            win=$(wmctrl -lp | awk -v p=$pid '$3 == p {print $1; exit}')
            [ -n "$win" ] && break
            sleep 0.1
        done
        if [ -n "$win" ]; then
            wmctrl -i -r "$win" -e "0,$(( i * W )),0,$W,$H"
            xdotool windowmove "$win" $(( i * W )) 0
            xdotool set_window --name "The Mall - seed $seed" "$win" 2>/dev/null
        fi
        local start=$SECONDS shot=0
        while kill -0 $pid 2>/dev/null; do
            if (( !shot && SECONDS - start >= SECONDS_EACH - 3 )) && [ -n "$win" ]; then
                import -window "$win" "$OUT/$seed.png" 2>/dev/null; shot=1
            fi
            if (( SECONDS - start > SECONDS_EACH + 30 )); then kill $pid; fi
            sleep 1
        done
        wait $pid; local code=$?
        if (( code == 0 && SECONDS - start >= SECONDS_EACH - 2 )); then
            echo "$(date +%T) slot $i seed $seed ran ${SECONDS_EACH}s"
        else
            echo "$(date +%T) slot $i seed $seed EXITED code $code after $(( SECONDS - start ))s" | tee -a "$OUT/crashes.txt"
        fi
        seed=$(( seed + SLOTS ))
    done
}

trap 'pkill -P $$; pkill -f "build/themall --seed"; exit' INT TERM
for (( i = 0; i < SLOTS; i++ )); do slot $i $(( SEED + i )) & done
wait
