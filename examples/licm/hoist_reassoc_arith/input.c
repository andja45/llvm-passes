void process(float *arr, int n, float offset, float base) {
    for (int i = 0; i < n; i++) {
        arr[i] = (arr[i] + offset) + base;
    }
}
