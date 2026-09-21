#include <assert.h>
#include <stdio.h>
#include "checksum.h"

void test_empty_data_gives_zero(void) {
    uint8_t data[] = {};
    assert(simple_checksum(data, 0) == 0);
}

void test_known_values_sum_correctly(void) {
    uint8_t data[] = {1, 2, 3};
    assert(simple_checksum(data, 3) == 7);  // wrong on purpose — real answer is 6
}

int main(void) {
    test_empty_data_gives_zero();
    test_known_values_sum_correctly();
    printf("All tests passed!\n");
    return 0;
}
