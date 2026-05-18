# LICM Pass — Examples

Each example targets one feature. Before = source as written. After = what the pass produces, expressed in C.

---

## 1. Core Hoisting (`hoist_invariant`)

Invariant arithmetic moved before the loop — computed once instead of n times.

**Before**
```c
void transform(int *arr, int n, int a, int b, int c) {
    for (int i = 0; i < n; i++) {
        arr[i] = (a * b + c) * i;   // a*b and a*b+c are invariant — recomputed every iteration
    }
}
```

**After**
```c
void transform(int *arr, int n, int a, int b, int c) {
    int inv = a * b + c;            // hoisted — computed once
    for (int i = 0; i < n; i++) {
        arr[i] = inv * i;
    }
}
```

---

## 2. Load Hoisting (`hoist_load`)

A load through a loop-invariant pointer with no aliasing stores is hoisted — one memory read instead of n.

**Before**
```c
void scale(int * __restrict__ arr, int n, const int * __restrict__ factor) {
    for (int i = 0; i < n; i++) {
        arr[i] *= *factor;          // *factor read from memory every iteration
    }
}
```

**After**
```c
void scale(int * __restrict__ arr, int n, const int * __restrict__ factor) {
    int f = *factor;                // hoisted — one load before the loop
    for (int i = 0; i < n; i++) {
        arr[i] *= f;
    }
}
```

---

## 3. Memory Promotion (`hoist_promotion`)

A load/store pair to the same address is replaced by a register — one load before, one store after, pure register arithmetic inside.

**Before**
```c
void accumulate(int * __restrict__ arr, int n, int * __restrict__ total) {
    for (int i = 0; i < n; i++) {
        *total += arr[i];           // load and store *total every iteration
    }
}
```

**After**
```c
void accumulate(int * __restrict__ arr, int n, int * __restrict__ total) {
    int reg = *total;               // load once before loop
    for (int i = 0; i < n; i++) {
        reg += arr[i];              // register arithmetic — no memory access
    }
    *total = reg;                   // store once after loop
}
```

---

## 4. Pure Call Hoisting (`hoist_call`)

Calls declared `const` (readnone) with loop-invariant arguments are hoisted — called once instead of n times.

**Before**
```c
double compute(double x) __attribute__((const));
double transform(double y) __attribute__((const));

void process(double *arr, int n, double x, double y) {
    for (int i = 0; i < n; i++) {
        arr[i] = arr[i] * compute(x) + transform(y);   // two calls per iteration
    }
}
```

**After**
```c
void process(double *arr, int n, double x, double y) {
    double cx = compute(x);         // hoisted — called once
    double ty = transform(y);       // hoisted — called once
    for (int i = 0; i < n; i++) {
        arr[i] = arr[i] * cx + ty;
    }
}
```

---

## 5. Instruction Sinking (`sink`)

An instruction whose result is only used after the loop is moved to the exit block — executed once instead of n times.

> Note: C → IR conversion (mem2reg) introduces PHI nodes that block sinking demos. The concept is shown below in C terms; in practice this is demonstrated at the IR level.

**Before**
```c
int process(int *arr, int n) {
    int last;
    for (int i = 0; i < n; i++) {
        last = arr[i] * 2;          // only the final value is used — computed n times for nothing
    }
    return last;
}
```

**After**
```c
int process(int *arr, int n) {
    int last_val;
    for (int i = 0; i < n; i++) {
        last_val = arr[i];          // load stays in loop
    }
    return last_val * 2;            // mul sunk to exit — computed once
}
```

---

## 6. Reciprocal Division Hoisting (`hoist_reciprocal`)

`fdiv x, invariant` → compute `1.0/c` once in the preheader → replace with `fmul x, recip`. `fmul` throughput is up to 40x higher than `fdiv`.

**Before**
```c
void normalize(float * __restrict__ arr, int n, float scale) {
    for (int i = 0; i < n; i++) {
        arr[i] = arr[i] / scale;    // n divisions
    }
}
```

**After**
```c
void normalize(float * __restrict__ arr, int n, float scale) {
    float recip = 1.0f / scale;     // one division hoisted
    for (int i = 0; i < n; i++) {
        arr[i] = arr[i] * recip;    // n multiplications
    }
}
```

---

## 7. Arithmetic Reassociation (`hoist_reassoc_arith`)

Rewrites `(varying + inv1) + inv2` → `varying + (inv1 + inv2)` to expose the invariant sub-expression, then hoists it. Regular hoisting misses this because the outer add appears non-invariant.

**Before**
```c
void process(float *arr, int n, float offset, float base) {
    for (int i = 0; i < n; i++) {
        arr[i] = (arr[i] + offset) + base;   // two fadds per iteration
    }
}
```

**After**
```c
void process(float *arr, int n, float offset, float base) {
    float reassoc = offset + base;            // one fadd hoisted
    for (int i = 0; i < n; i++) {
        arr[i] = arr[i] + reassoc;            // one fadd per iteration
    }
}
```

---

## 8. GEP Reassociation (`hoist_reassoc_gep`)

Splits `getelementptr base, (i + offset)` into a preheader GEP that advances the base by the invariant offset, and a loop GEP that steps only by `i`. One fewer add per iteration.

**Before**
```c
void copy(float * __restrict__ dst, float * __restrict__ src, long n, long offset) {
    for (long i = 0; i < n; i++) {
        dst[i] = src[i + offset];   // i + offset computed every iteration
    }
}
```

**After**
```c
void copy(float * __restrict__ dst, float * __restrict__ src, long n, long offset) {
    float *base = src + offset;     // GEP advanced by offset once in preheader
    for (long i = 0; i < n; i++) {
        dst[i] = base[i];           // only the varying index remains
    }
}
```

---

## 9. SE-Unlocked Hoisting (`hoist_se`)

`isSafeToSpeculativelyExecute` rejects integer division (potential div-by-zero). Regular hoisting skips it. The for-loop body never dominates the exit (zero-trip path `header → exit` bypasses it), so the dominator fallback also fails. SE proves the loop runs exactly 4 times → body executes at least once → safe to hoist.

**Before**
```c
void scale(int * __restrict__ out, const int * __restrict__ in,
           int a, int b) {
    for (int i = 0; i < 4; i++) {
        out[i] = in[i] * (a / b);   // a/b invariant but sdiv unsafe — 4 divisions
    }
}
```

**After**
```c
void scale(int * __restrict__ out, const int * __restrict__ in,
           int a, int b) {
    int q = a / b;                  // SE-unlocked hoist — one division before the loop
    for (int i = 0; i < 4; i++) {
        out[i] = in[i] * q;
    }
}
```
