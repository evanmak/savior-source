#include <stdio.h>

void foo(int index) {
    int arr[4] = {0, 1, 2, 3};
    if (index < 4) {
        int* p = arr + index; // no out of bound
    }
}

int main(int argc, char const *argv[])
{
    /* code */
    int index = 10;
    foo(index);
    return 0;
}
