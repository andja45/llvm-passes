# LLVM Pass Collection

Custom LLVM optimization pass plugins built on LLVM's analysis infrastructure -
DominatorTree, AliasAnalysis, ScalarEvolution, and LoopInfo.

**Tech focus:**  
LLVM 18 · C++17 · CMake · New Pass Manager · Pass Plugin API

**Pass focus:**  
Loop-Invariant Code Motion · Jump Threading · SimplifyCFG · DSE

---

## LICM - Loop-Invariant Code Motion

Moves loop-invariant computation out of loops - repeats while any
transformation fires; hoisting can expose new invariants each pass.

| Feature                                                                                               | Analyses |
|-------------------------------------------------------------------------------------------------------|--|
| **Preheader insertion** - ensure loop preheader exists before hoisting                                | DT · LI |
| **Core hoisting** - hoist loop-invariant instructions with safety checks                              | DT · LI |
| **Load hoisting** - hoist loop-invariant loads using alias analysis                                   | AA · LI |
| **Call hoisting** - hoist readonly/readnone calls with invariant arguments                            | AA · LI |
| **Memory promotion** - promote loop memory accesses to SSA registers via PHI-based load-store elimination | AA · DT · LI |
| **Arithmetic + GEP reassociation** - hoist hidden invariants via arithmetic and GEP reassociation     | LI |
| **Reciprocal multiplication** - hoist reciprocal of loop-invariant divisor to preheader               | LI |
| **Instruction sinking** - sink instructions used only in loop exit blocks                             | DT · LI |
| **SE-unlocked hoist/sink** - SE-unlocked hoisting and sinking of non-memory instructions unsafe to speculate `*` | SE · DT |
| **LCSSA maintenance** - maintain LCSSA form after all loop transformations                            | DT · LI · SE |
| **Statistics + debug** - pass statistics and debug logging                                            | - |

`*` LLVM covers do-while via dominator-based execution guarantees; the zero-trip
path blocks that for for/while loops. SE proving TC > 0 guarantees the first
iteration; every path from header to latch through BB guarantees no iteration
skips it - covers the case LLVM conservatively skips.

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

## DSE — Dead Store Elimination

Removes redundant memory stores that are overwritten before being used.
This pass performs a simple dead store elimination on LLVM IR.
It detects stores to local variables that are overwritten before any intervening use and removes the redundant stores. The implementation also preserves stores that are read before being overwritten or whose addresses are passed to function calls, since their values may be observed externally.

| Feature | LLVM IR |
|---------|---------|
| **Dead store detection** — identify stores whose values are overwritten before being read | StoreInst · LoadInst |
| **Multiple variable tracking** — track stores independently for different memory locations | StoreInst · AllocaInst · DenseMap |
| **Load/use checking** — preserve stores when the stored value is read before being overwritten | LoadInst |
| **Function call handling** — preserve stores when the address of a variable is passed to a function call | CallInst · Value |
| **End-of-function dead store removal** — remove stores whose values are never used afterwards | StoreInst · Function |
| **IR transformation** — delete redundant store instructions from LLVM IR | Instruction · BasicBlock |

**Tech focus:** LLVM IR, `StoreInst`, `LoadInst`, `CallInst`, `BasicBlock`, `FunctionPass`, memory access analysis.

See [dse/README.md](dse/README.md) for full breakdown with before/after IR and CFG.

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
# compile to IR (replace <example> with e.g. basic, different-condition, different-variable, goto)
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
| SimplifyCFG | [Ana Stevanovic](https://github.com/AnaStevanovic) |
| Jump Threading | [Dunja Milenkovic](https://github.com/DunjaMilenkovic) |
| DSE | [Natalija Pavlicevic](https://github.com/natalijapavlicevic) |
