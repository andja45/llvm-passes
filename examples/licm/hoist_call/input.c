double compute(double x) __attribute__((const));   // readnone (no memory access, deterministic output)
double transform(double y) __attribute__((const));

void process(double *arr, int n, double x, double y) {
    for (int i = 0; i < n; i++) {
        arr[i] = arr[i] * compute(x) + transform(y);
    }
}
