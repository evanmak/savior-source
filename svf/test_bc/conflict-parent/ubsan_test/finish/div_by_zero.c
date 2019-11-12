#include <stdio.h>
#include <limits.h>

void foo(int c) {
    if (c != 0) {
        int d = 10 / c;
    }
}

int main(int argc, char const *argv[])
{
    foo(0);
    return 0;
}
