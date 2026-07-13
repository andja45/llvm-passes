void scale(int * __restrict__ arr, int n, const int * __restrict__ factor) { // __restrict__ (pointer has no aliases)
    for (int i = 0; i < n; i++) {
        arr[i] *= *factor;
    }
}
