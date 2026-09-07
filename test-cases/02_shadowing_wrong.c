#include <stdio.h>

int main(void) {
    int y = 5;
    {
        int y = 10;
        printf("%d\n", y);
    }
    printf("%d\n", y);
    return 0;
}
