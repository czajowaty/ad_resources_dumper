#ifndef BITSREADER_HPP
#define BITSREADER_HPP

#include <cstdint>

class BitsReader
{
public:
    BitsReader(uint8_t const* buffer);

    uint8_t const* currentBytePtr() const;
    uint8_t readBit();
    uint8_t readBool();

private:
    void bufferNextByte();

    uint8_t const* currentBytePtr_;
    uint8_t bitIndex_;
    uint8_t bufferedByte_;
};

#endif // BITSREADER_HPP
