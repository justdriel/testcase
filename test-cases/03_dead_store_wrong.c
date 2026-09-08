#include <stdio.h>

int main(void) {
    int a;
    a = 5;
    a = 10;      // clearer dead store — first assignment never read
    printf("%d\n", a);
    return 0;
}
