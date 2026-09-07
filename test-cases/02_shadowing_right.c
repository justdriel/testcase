#include <stdio.h>

int main(void) {
    int outer_y = 5;
    {
        int inner_y = 10;
        printf("%d\n", inner_y);
    }
    printf("%d\n", outer_y);
    return 0;
}
