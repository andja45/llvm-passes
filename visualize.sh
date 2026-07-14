#!/bin/bash
set -euo pipefail

PROJECT="$(cd "$(dirname "$0")" && pwd)"

# Register passes here
declare -A PLUGIN_NAME=(
    [licm]="LICM"
    [simplifycfg]="SimplifyCFG"
    [jump-threading]="JumpThreading"
)

declare -A OPT_PASSES=(
    [licm]="mem2reg,loop(licm-pass)"
    [simplifycfg]="simplifycfg-pass"
    [jump-threading]="my-jump-threading"
)

PASSES=("${!PLUGIN_NAME[@]}")
[ $# -gt 0 ] && PASSES=("$@")

# Generates CFG visualizations
#
# For a module containing one function:
#   original.png
#   optimized.png
#
# For a module containing multiple functions:
#   original-<function>.png
#   optimized-<function>.png
generate_cfg() {
    local input="$1"
    local prefix="$2"

    opt -passes=dot-cfg -disable-output "$input" 2>/dev/null

    # Prevent an unmatched pattern from remaining as the literal string ".*.dot"
    shopt -s nullglob
    local dot_files=(.*.dot)
    shopt -u nullglob

    local count="${#dot_files[@]}"

    if [ "$count" -eq 0 ]; then
        return
    fi

    for dot_file in "${dot_files[@]}"; do
        local clean="${dot_file#.}"
        local base="${clean%.dot}"
        local output

        mv "$dot_file" "$clean"

        if [ "$count" -eq 1 ]; then
            output="${prefix}.png"
        else
            output="${prefix}-${base}.png"
        fi

        dot -Tpng -Gdpi=150 -Nfontsize=11 \
            "$clean" \
            -o "$output"

        rm "$clean"
    done
}

run_pass() {
    local pass="$1"

    if [ -z "${PLUGIN_NAME[$pass]+x}" ] ||
       [ -z "${OPT_PASSES[$pass]+x}" ]; then
        echo "  [!] unknown pass: $pass"
        return
    fi

    local plugin="$PROJECT/cmake-build-debug/$pass/${PLUGIN_NAME[$pass]}.so"
    local examples_dir="$PROJECT/examples/$pass"

    echo "==> $pass"

    if [ ! -f "$plugin" ]; then
        echo "  [!] plugin not found — run: cmake --build cmake-build-debug --target ${PLUGIN_NAME[$pass]}"
        return
    fi

    if [ ! -d "$examples_dir" ]; then
        echo "  [!] examples directory not found: $examples_dir"
        return
    fi

    # Avoid processing the same directory twice if it contains both input.c
    # and input.ll. input.c takes precedence in that case
    declare -A processed_dirs=()

    while IFS= read -r -d '' input; do
        local dir
        local name
        local before

        dir="$(dirname "$input")"

        if [ -n "${processed_dirs[$dir]+x}" ]; then
            continue
        fi

        processed_dirs["$dir"]=1

        name="${dir#"$examples_dir/"}"
        echo "  --> $name"

        if [ -f "$dir/input.c" ]; then
            clang -S -emit-llvm -O0 \
                -Xclang -disable-O0-optnone \
                -fno-discard-value-names \
                "$dir/input.c" \
                -o "$dir/original.ll"
        elif [ -f "$dir/input.ll" ]; then
            cp "$dir/input.ll" "$dir/original.ll"
        else
            continue
        fi

        if ! opt \
            --load-pass-plugin="$plugin" \
            --passes="${OPT_PASSES[$pass]}" \
            "$dir/original.ll" \
            -S \
            -o "$dir/optimized.ll" \
            > "$dir/pass.log" 2>&1; then

            echo "  [!] pass failed — see $dir/pass.log"
            continue
        fi

        before="$(mktemp --suffix=.ll)"

        opt --passes="mem2reg" \
            "$dir/original.ll" \
            -S \
            -o "$before"

        (
            cd "$dir"

            # Remove old visualizations so stale images do not remain after
            # function names or the number of functions change
            rm -f \
                original.png \
                optimized.png \
                original-*.png \
                optimized-*.png \
                before-*.png \
                after-*.png

            generate_cfg "$before" original
            generate_cfg optimized.ll optimized
        )

        rm -f "$before"
    done < <(
        find "$examples_dir" \
            -type f \
            \( -name "input.c" -o -name "input.ll" \) \
            -print0
    )
}

for pass in "${PASSES[@]}"; do
    run_pass "$pass"
done

echo "Done."