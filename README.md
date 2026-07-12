# LLVM Pass Collection

Custom LLVM optimization pass plugins built on LLVM's analysis infrastructure —
DominatorTree, AliasAnalysis, ScalarEvolution, and LoopInfo.

**Tech focus:**  
LLVM 18 · C++17 · CMake · New Pass Manager · Pass Plugin API

**Pass focus:**  
Loop-Invariant Code Motion · Jump Threading · [Pass 3] · [Pass 4]

---

## LICM — Loop-Invariant Code Motion

Moves loop-invariant computation out of loops — repeats while any
transformation fires; hoisting can expose new invariants each pass.

| Feature                                                                                               | Analyses |
|-------------------------------------------------------------------------------------------------------|--|
| **Preheader insertion** — ensure loop preheader exists before hoisting                                | DT · LI |
| **Core hoisting** — hoist loop-invariant instructions with safety checks                              | DT · LI |
| **Load hoisting** — hoist loop-invariant loads using alias analysis                                   | AA · LI |
| **Call hoisting** — hoist readonly/readnone calls with invariant arguments                            | AA · LI |
| **Memory promotion** — promote loop memory accesses to SSA registers via PHI-based load-store elimination | AA · DT · LI |
| **Arithmetic + GEP reassociation** — hoist hidden invariants via arithmetic and GEP reassociation     | LI |
| **Reciprocal multiplication** — hoist reciprocal of loop-invariant divisor to preheader               | LI |
| **Instruction sinking** — sink instructions used only in loop exit blocks                             | DT · LI |
| **SE-unlocked hoist/sink** — SE-unlocked hoisting and sinking of non-memory instructions unsafe to speculate `*` | SE · DT |
| **LCSSA maintenance** — maintain LCSSA form after all loop transformations                            | DT · LI · SE |
| **Statistics + debug** — pass statistics and debug logging                                            | — |

`*` LLVM covers do-while via dominator-based execution guarantees; the zero-trip
path blocks that for for/while. SE proving TC > 0 guarantees the first
iteration; every path from header to latch through BB guarantees no iteration
skips it — covers the case LLVM conservatively skips.

See [licm/README.md](licm/README.md) for full breakdown with before/after IR and CFG.

---

## Jump Threading  — Eliminate redundant conditional branches

Detects repeated conditional branches evaluating the same comparison and redirects
CFG edges to bypass redundant basic blocks.

**Tech focus:** CFG · LLVM IR · BranchInst · ICmpInst

| Feature | Analyses |
|-----------------------------------------------------------------------------------------------------|--|
| **Threading candidate detection** — find predecessor/current block pairs with equivalent conditions | CFG |
| **Comparison matching** — verify identical comparison predicate, variable and constant              | LLVM IR |
| **Edge redirection** — redirect predecessor branch to the final destination                         | CFG |
| **Redundant branch elimination** — bypass redundant conditional blocks                                         | CFG |
| **Debug logging** — print detected candidates and performed transformations | — |

See [jump-threading/README.md](jump-threading/README.md) for full breakdown with before/after IR and CFG.

---

## [Pass Name] — [one-line description]

[Brief description of what the pass optimizes and how.]

**Tech focus:** [analyses / IR constructs used]

See [pass-dir/README.md] for full breakdown.

---

## [Pass Name] — [one-line description]

[Brief description of what the pass optimizes and how.]

**Tech focus:** [analyses / IR constructs used]

See [pass-dir/README.md] for full breakdown.

---

## Build

**Requirements:** LLVM 18+, CMake 3.16+, C++17 compiler, Graphviz (for `visualize.sh`)

```bash
git clone https://github.com/andja45/llvm-passes.git
cd llvm-passes
cmake -B cmake-build-debug
cmake --build cmake-build-debug --target LICM
```

Run all examples — generates before/after IR and CFG PNGs for every feature:
```bash
./visualize.sh
```

Run a single example manually:
```bash
# compile to IR  (replace <example> with e.g. hoist_se, hoist_load, ...)
clang -S -emit-llvm -O0 -Xclang -disable-O0-optnone -fno-discard-value-names \
    examples/licm/<example>/input.c -o examples/licm/<example>/original.ll

# run pass
opt --load-pass-plugin=./cmake-build-debug/licm/LICM.so \
    --passes="mem2reg,loop(licm-pass)" \
    examples/licm/<example>/original.ll -S -o examples/licm/<example>/optimized.ll
```

```bash
# compile to IR (replace <example> with e.g. basic, different-condition, ...)
clang -S -emit-llvm -O0 -Xclang -disable-O0-optnone -fno-discard-value-names \
    examples/jump-threading/<example>/input.c \
    -o examples/jump-threading/<example>/original.ll

# run pass
opt --load-pass-plugin=./cmake-build-debug/jump-threading/JumpThreading.so \
    --passes="my-jump-threading" \
    examples/jump-threading/<example>/original.ll \
    -S -o examples/jump-threading/<example>/optimized.ll
```

---

## Authors

| Pass | Author |
|---|---|
| LICM | [Andjela Spasic](https://github.com/andja45) |
| [Pass 2] | [Dunja Milenkovic](https://github.com/DunjaMilenkovic) |
| [Pass 3] | [[Name]](https://github.com/username) |
| [Pass 4] | [[Name]](https://github.com/username) |