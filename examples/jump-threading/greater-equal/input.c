int foo(int x) {

    if (x >= 5) {

        if (x >= 5)
            return 1;
    }

    return 2;
}

int main() {
    return foo(5);
}