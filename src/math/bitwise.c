
// Packs two integers given the number of bits allocated to the low value
int PackInts(int low, int high, int low_bits)
{
    // Generate masks dynamically
    // E.g., if low_bits is 26, (1 << 26) - 1 creates the binary mask 0x03FFFFFF
    int low_mask = (1 << low_bits) - 1;
    int high_bits = 32 - low_bits;
    int high_mask = (1 << high_bits) - 1;

    int masked_low = low & low_mask;
    int shifted_high = (high & high_mask) << low_bits;

    return shifted_high | masked_low;
}

// Unpacks the low value from the bottom bits
int UnpackInts_Low(int packed_value, int low_bits)
{
    int low_mask = (1 << low_bits) - 1;
    return packed_value & low_mask;
}

// Unpacks the high value from the top bits
int UnpackInt_High(int packed_value, int low_bits)
{
    return (packed_value >> low_bits) & ((1 << (32 - low_bits)) - 1);
}

int PackInts_26_6(int low, int high)
{
    // Mask A to ensure it fits in 4 bits (0xF is 1111 in binary), then shift it left by 26
    int shifted_a = (low & 0x3F) << 26;

    // Mask B to ensure it fits in 26 bits (0x0FFFFFFF clears the top 6 bits)
    int masked_b = high & 0x03FFFFFF;

    // Merge them
    return shifted_a | masked_b;
}

// Unpack the Type from the packed int
int UnpackInt(int packed_value, int offset)
{
    // Shift right by offset bits to isolate the offset bits
    return packed_value >> offset;
}

// Unpack the right-half of the packed int
int UnpackLowInt(int packed_value, int offset)
{
    // Mask off the high bits to isolate the low 16 bits
    return packed_value & 0xFFFF;
}
