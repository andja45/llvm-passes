int foo(int x) {

    if (x > 0)
        goto L;

    return 3;

L:

    if (x > 0)
        return 1;

    return 2;
}

int main() {
    return foo(5);
}