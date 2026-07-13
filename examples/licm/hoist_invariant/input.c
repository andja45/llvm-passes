void transform(int *arr, int n, int a, int b, int c) {
    for (int i = 0; i < n; i++) {
        arr[i] = (a * b + c) * i;
    }
}
