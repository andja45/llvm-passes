# SimplifyCFG — Control-Flow Graph Simplification

A lightweight LLVM pass implementing a subset of control-flow graph (CFG)
simplifications. The pass performs a sequence of lightweight CFG
transformations that remove redundant basic blocks and simplify SSA form
while preserving program semantics.

**Tech focus:**  
LLVM 18 · C++17 · LLVM IR · New Pass Manager · Pass Plugin API

---

## Implemented transformations

- Remove unreachable basic blocks
- Merge consecutive basic blocks
- Remove trivial branch blocks
- Simplify PHI nodes in blocks with a single predecessor

---

## Build and run

Build the pass:

```bash
cmake -B cmake-build-debug
cmake --build cmake-build-debug --target SimplifyCFG
```

Run the pass manually:

```bash
opt --load-pass-plugin=./cmake-build-debug/simplifycfg/SimplifyCFG.so \
    --passes="simplifycfg-pass" \
    input.ll -S -o optimized.ll
```

Generate LLVM IR and CFG visualizations for all examples:

```bash
./visualize.sh simplifycfg
```

---

## Transformations

### Remove unreachable basic blocks

**Description**

Removes basic blocks that cannot be reached from the function entry block.
Incoming edges from removed blocks are also removed from successor PHI nodes.

**Conditions**

- the block is unreachable from the function entry block

---

### Merge consecutive basic blocks

**Description**

Merges two consecutive basic blocks when the predecessor has a single successor
and the successor has a single predecessor. PHI nodes are simplified before the
merge.

**Conditions**

- predecessor has exactly one successor
- successor has exactly one predecessor

---

### Remove trivial branch blocks

**Description**

Removes basic blocks containing only an unconditional branch by redirecting all
incoming edges to the successor and updating successor PHI nodes.

**Conditions**

- block contains only an unconditional branch
- block is not the entry block
- block is not a self-loop

---

### Simplify single-predecessor PHI nodes

**Description**

Replaces PHI nodes in basic blocks with a single predecessor by their incoming
value and removes the redundant PHI instructions.

**Conditions**

- block has exactly one predecessor

---

## Test examples

Each example contains:

- input LLVM IR
- generated original LLVM IR
- optimized LLVM IR
- original CFG visualization
- optimized CFG visualization

| Transformation | Examples |
|----------------|----------|
| Remove unreachable basic blocks | simple · cycle · phi |
| Merge basic blocks | simple · phi-update · multiple-successors · multiple-predecessors |
| Remove trivial branch blocks | conditional-predecessor · multiple-predecessors · self-loop |
| Simplify single-predecessor PHI nodes | single-phi · multiple-phis · multiple-predecessors |