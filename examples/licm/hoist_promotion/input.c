void accumulate(int * __restrict__ arr, int n, int * __restrict__ total) { // __restrict__ (pointer has no aliases)
    for (int i = 0; i < n; i++) {
        *total += arr[i];
    }
}
