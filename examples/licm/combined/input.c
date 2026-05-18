double compute(double x) __attribute__((const));

void pipeline(float * __restrict__ dst, const float * __restrict__ src, float * __restrict__ acc, long offset,
              float scale, float base, int a, int b, double cx, const float * __restrict__ factor) {
    for (long i = 0; i < 4; i++) {
        int coeff = a * b + 1;                // core hoisting — invariant arithmetic
        float raw = src[i + offset];          // GEP reassociation — splits offset to preheader
        float normed = raw / scale;           // reciprocal multiplication — 1/scale hoisted
        float adj = (normed + base) + scale;  // arithmetic reassociation — base+scale hoisted
        float q = (float)(a / b);             // SE-unlocked — sdiv unsafe, SE proves TC=4
        float boost = (float)compute(cx);     // call hoisting — readnone, invariant arg
        float f = *factor;                    // load hoisting — invariant ptr, no aliasing store
        *acc += normed * q * (float)coeff;    // memory promotion — *acc -> register, PHI carries value across iterations
        dst[i] = adj * boost * f;             // arithmetic reassociation — boost*f hoisted as invariant fmul pair
    }
}
