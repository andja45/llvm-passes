# Dead Store Elimination

Simplified implementation of the LLVM Dead Store Elimination optimization pass.

The pass detects redundant memory stores in LLVM IR and removes stores whose values are overwritten before being read. The implementation tracks stores to local variables and eliminates previous stores when no intervening load or observable use exists.

---

## Implemented Features

| Feature | Description |
|---------|-------------|
| **Dead store detection** | Detect stores whose values are overwritten before being used. |
| **Multiple variable support** | Track stores independently for different local variables. |
| **Load detection** | Preserve stores when the stored value is loaded before being overwritten. |
| **Function call handling** | Preserve stores when a pointer to the stored memory location is passed to a function call. |
| **End-of-function dead stores** | Remove stores whose values are never used afterwards. |
| **IR transformation** | Remove redundant `StoreInst` instructions from the LLVM IR. |
| **Debug logging** | Print detected dead stores and performed transformations. |

---

## Limitations

This simplified implementation supports only a subset of LLVM's full Dead Store Elimination optimization.

Current assumptions:

- only local variables allocated using `alloca` are considered
- analysis is performed inside a single function
- complex control-flow situations (e.g. branches with multiple execution paths) are not optimized
- no alias analysis is performed
- indirect memory accesses are conservatively handled
- interprocedural effects are not analyzed

---

## Example

### Input

```c
int main() {

    int x;

    x = 1;
    x = 2;

    return x;
}
```

---

### Before

```llvm
store i32 1, ptr %x
store i32 2, ptr %x

%value = load i32, ptr %x
ret i32 %value
```

The first store is overwritten before being used.

---

### After

```llvm
store i32 2, ptr %x

%value = load i32, ptr %x
ret i32 %value
```

The redundant store is removed.

---

## Test Examples

| Example | Description |
|---------|-------------|
| **dead-simple** | Two consecutive stores to the same variable — first store is removed. |
| **multiple-variables** | Independent tracking of stores for multiple local variables. |
| **load-used** | Store is preserved because the value is loaded before being overwritten. |
| **function-call-pointer** | Store is preserved when the variable address is passed to another function. |
| **dead-at-end** | Store is removed when the value is never used afterwards. |

---

## Visualization

For every example the repository contains:

- input C source
- original LLVM IR
- optimized LLVM IR
- optimization logs

Generate optimized output with:

```bash
opt \
-load-pass-plugin ./build/dse/DSE.so \
-passes=my-dse \
-S example.ll \
-o optimized.ll
```

---

## Source Files

```
dse/
├── DSE.cpp
├── CMakeLists.txt
├── README.md
└── build/
```