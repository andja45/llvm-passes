void copy(float * __restrict__ dst, float * __restrict__ src, long n, long offset) {
    for (long i = 0; i < n; i++) {
        dst[i] = src[i + offset];
    }
}
