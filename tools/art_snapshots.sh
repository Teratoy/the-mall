#!/bin/bash
# Saves a set of pictures of the game's art to one folder, to compare before
# and after a change meant to draw the same: shots of many worlds, revealed,
# close and far, with piles down and the wide layout (its map), plus the
# vending sheet. Run inside the toolbox after make:
#   tools/art_snapshots.sh build/art-before   ...change...   tools/art_snapshots.sh build/art-after
# Water glints follow the wall clock, so water pixels may differ between any two runs.
#   cmp each pair, e.g. for f in build/art-before/*; do cmp -s "$f" "build/art-after/${f##*/}" || echo "$f"; done
set -e
cd "$(dirname "$0")/.."
OUT=${1:?usage: tools/art_snapshots.sh outdir}
mkdir -p "$OUT"
make -s build/shot
g++ -std=c++20 -O2 -Isrc tools/vending.cpp $(find build -path build/android -prune -o -name "*.o" ! -name main.o -print) \
    -o build/vending $(pkg-config --libs raylib) -lm
./build/vending "$OUT/vending.png" >/dev/null 2>&1
for seed in 1 2 3 4 5 6 7 8 9 10 11 12; do
    for secs in 5 60 240; do
        ./build/shot "$OUT/s$seed-t$secs-z3.png" --seed $seed --seconds $secs --zoom 3 --reveal --pile >/dev/null 2>&1
        ./build/shot "$OUT/s$seed-t$secs-z1.png" --seed $seed --seconds $secs --zoom 1 --reveal >/dev/null 2>&1
        ./build/shot "$OUT/s$seed-t$secs-wide.png" --seed $seed --seconds $secs --wide >/dev/null 2>&1
    done
done
ls "$OUT" | wc -l
