#ifndef BITSHELPER_HPP
#define BITSHELPER_HPP

#include <cstdint>

static constexpr uint8_t BITS_IN_BYTE = 8;

inline constexpr uint32_t valuesInBits(uint8_t bits)
{ return 1 << bits; }

#endif // BITSHELPER_HPP
