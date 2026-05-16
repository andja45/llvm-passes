void normalize(float * __restrict__ arr, int n, float scale) {
    for (int i = 0; i < n; i++) {
        arr[i] = arr[i] / scale;
    }
}
