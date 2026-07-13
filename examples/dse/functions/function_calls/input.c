#include <stdio.h>

int global = 0;

void foo() {
    global = 100;
}

int main() {

    int x;

    x = 5;

    foo();

    x = 10;

    return x;
}