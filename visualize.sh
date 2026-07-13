#!/bin/bash
set -euo pipefail

PROJECT="$(cd "$(dirname "$0")" && pwd)"

# register passes here
declare -A PLUGIN_NAME=([licm]="LICM")
declare -A OPT_PASSES=([licm]="mem2reg,loop(licm-pass)")

PASSES=("${!PLUGIN_NAME[@]}")
[ $# -gt 0 ] && PASSES=("$@")

generate_cfg() {
    local input="$1" output="$2"
    opt -passes=dot-cfg -disable-output "$input" 2>/dev/null
    for DOT in .*.dot; do
        [ -f "$DOT" ] || continue
        CLEAN="${DOT#.}"
        mv "$DOT" "$CLEAN"
        dot -Tpng -Gdpi=150 -Nfontsize=11 "$CLEAN" -o "$output"
        rm "$CLEAN"
    done
}

run_pass() {
    local PASS="$1"

    if [[ ! -v PLUGIN_NAME[$PASS] ]]; then
        echo "  [!] unknown pass: $PASS"
        return
    fi

    local PLUGIN="$PROJECT/cmake-build-debug/$PASS/${PLUGIN_NAME[$PASS]}.so"

    echo "==> $PASS"

    if [ ! -f "$PLUGIN" ]; then
        echo "  [!] plugin not found — run: cmake --build cmake-build-debug --target ${PLUGIN_NAME[$PASS]}"
        return
    fi

    for DIR in "$PROJECT/examples/$PASS"/*/; do
        [ -f "$DIR/input.c" ] || continue
        NAME=$(basename "$DIR")
        echo "  --> $NAME"

        clang -S -emit-llvm -O0 -Xclang -disable-O0-optnone \
              -fno-discard-value-names \
              "$DIR/input.c" -o "$DIR/original.ll"

        if ! opt --load-pass-plugin="$PLUGIN" \
                --passes="${OPT_PASSES[$PASS]}" \
                "$DIR/original.ll" -S -o "$DIR/optimized.ll" \
                > "$DIR/pass.log" 2>&1; then
            echo "  [!] pass failed — see $DIR/pass.log"
            continue
        fi

        BEFORE=$(mktemp --suffix=.ll)
        opt --passes="mem2reg" "$DIR/original.ll" -S -o "$BEFORE"

        cd "$DIR"
        generate_cfg "$BEFORE" original.png
        generate_cfg optimized.ll optimized.png
        rm -f "$BEFORE"
        cd "$PROJECT"
    done
}

for PASS in "${PASSES[@]}"; do
    run_pass "$PASS"
done

echo "Done."
