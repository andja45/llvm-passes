#include <stdio.h>

void foo(int *p) {
    printf("%d\n", *p);
}

int main() {

    int x;
    int y;

    x = 1;
    y = 2;

    foo(&x);

    x = 3;
    y = 4;

    return 0;
}