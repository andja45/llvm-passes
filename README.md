# LLVM Pass Collection

Custom LLVM optimization pass plugins built on LLVM's analysis infrastructure —
DominatorTree, AliasAnalysis, ScalarEvolution, and LoopInfo.

**Tech focus:**  
LLVM 18 · C++17 · CMake · New Pass Manager · Pass Plugin API

**Pass focus:**  
Loop-Invariant Code Motion · [Pass 2] · SimplifyCFG · [Pass 4]

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

## [Pass Name] — [one-line description]

[Brief description of what the pass optimizes and how.]

**Tech focus:** [analyses / IR constructs used]

See [pass-dir/README.md] for full breakdown.

---

## SimplifyCFG — Control-flow graph simplification

Performs a sequence of lightweight CFG simplifications to eliminate redundant
basic blocks and simplify SSA form.

| Feature | LLVM IR |
|----------|---------|
| **Unreachable block removal** — remove unreachable basic blocks and update successor PHI nodes | BasicBlock · PHI |
| **Basic block merging** — merge consecutive basic blocks when CFG structure allows | BasicBlock · BranchInst · PHI |
| **Trivial branch elimination** — remove blocks containing only an unconditional branch | BranchInst · PHI |
| **Single-predecessor PHI simplification** — replace PHI nodes with their incoming value in blocks with a single predecessor | PHINode |

**Tech focus:**  
LLVM IR · BasicBlock · BranchInst · PHINode · SSA

See [simplifycfg/README.md](simplifycfg/README.md) for full breakdown with before/after IR and CFG.

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

---

## Authors

| Pass | Author |
|---|---|
| LICM | [Andjela Spasic](https://github.com/andja45) |
| [Pass 2] | [[Name]](https://github.com/username) |
| SimplifyCFG | [Ana Stevanovic](https://github.com/AnaStevanovic) |
| [Pass 4] | [[Name]](https://github.com/username) |
