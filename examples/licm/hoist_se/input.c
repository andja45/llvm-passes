void scale(int * __restrict__ out, const int * __restrict__ in, int a, int b) {
    for (int i = 0; i < 4; i++) {
        out[i] = in[i] * (a / b);
    }
}
