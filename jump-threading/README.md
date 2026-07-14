# Jump Threading

Simplified implementation of the LLVM Jump Threading optimization pass.

The pass detects repeated conditional branches evaluating the same comparison and redirects CFG edges to bypass redundant conditional blocks.

---

## Imlemented Features

| Feature | Description |
|---------|-------------|
| **Threading candidate detection** | Detect predecessor/current basic block pairs with equivalent conditional branches. |
| **Comparison matching** | Match identical comparison predicate, variable and constant value. |
| **Edge redirection** | Redirect predecessor branch directly to the final destination block. |
| **Debug logging** | Print detected threading candidates and performed CFG transformations. |

---

## Limitations

This simplified implementation supports only a subset of LLVM's full Jump Threading optimization.

Current assumptions:

- both conditions must use the same comparison predicate
- both conditions must compare the same loaded variable
- both conditions must compare against the same constant
- only conditional branches are considered
- unreachable blocks are not removed after redirection

---

## Example

### Input

```c
int foo(int x) {

    if(x > 0) {

        if(x > 0)
            return 1;
        else
            return 2;
    }

    return 3;
}
```

---

### Before

```
entry
 ├── true ──► check
 │             ├── true ─► return1
 │             └── false ─► return2
 └── false ─► return3
```

---

### After

```
entry
 ├── true ─────────────► return1
 └── false ───────────► return3
```

The second comparison is skipped because it evaluates the same condition as the predecessor.

---

## Test Examples

| Example | Description |
|---------|-------------|
| **basic** | Successful jump threading. |
| **different-condition** | Different comparisons — no optimization. |
| **different-variable** | Same predicate and constant, but different compared variable (`x` vs `y`) — no optimization. |
| **goto** | Threading through CFG with an explicit jump. |

---

## Visualization

For every example the repository contains:

- input C source
- original LLVM IR
- optimized LLVM IR
- CFG before optimization
- CFG after optimization

Generate all outputs with

```bash
./visualize.sh jump-threading
```

---

## Source Files

```
jump-threading/
├── JumpThreading.cpp
├── CMakeLists.txt
├── README.md
└── build/
```