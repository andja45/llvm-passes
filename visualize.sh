#!/bin/bash
set -euo pipefail

PROJECT="$(cd "$(dirname "$0")" && pwd)"
PLUGIN="$PROJECT/cmake-build-debug/licm/LICM.so"

if [ ! -f "$PLUGIN" ]; then
    echo "LICM plugin not found — run: cmake --build cmake-build-debug --target LICM"
    exit 1
fi

generate_cfg() {
    local input="$1"
    local output="$2"
    opt -passes=dot-cfg -disable-output "$input" 2>/dev/null
    for DOT in .*.dot; do
        [ -f "$DOT" ] || continue
        CLEAN="${DOT#.}"
        mv "$DOT" "$CLEAN"
        dot -Tpng -Gdpi=150 -Nfontsize=11 "$CLEAN" -o "$output"
        rm "$CLEAN"
    done
}

echo "==> LICM"
for DIR in "$PROJECT/examples/licm"/*/; do
    [ -f "$DIR/input.c" ] || continue
    NAME=$(basename "$DIR")
    echo "  --> $NAME"

    # -fno-discard-value-names preserves source variable names in IR and CFG
    clang -S -emit-llvm -O0 -Xclang -disable-O0-optnone \
          -fno-discard-value-names \
          "$DIR/input.c" -o "$DIR/original.ll"

    if ! opt --load-pass-plugin="$PLUGIN" \
            --passes="mem2reg,loop(licm-pass)" \
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

echo "Done."
