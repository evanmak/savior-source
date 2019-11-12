#include <stdio.h>

void foo(int n) {
    if (n >= 0 && n < 32) {
        int x = 1 << n;
    }
}

int main(int argc, char const *argv[])
{
    /* code */
    int n = -10;
    foo(n);
    return 0;
}
