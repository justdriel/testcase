uint32_t simple_checksum(const uint8_t *data, int length) {
    uint32_t sum = 0;
    for (int i = 0; i < length; i++) {
        sum += data[i];
    }
    return sum;
}
