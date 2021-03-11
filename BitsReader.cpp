#include "BitsReader.hpp"
#include "BitsHelper.hpp"

BitsReader::BitsReader(uint8_t const* buffer)
    : currentBytePtr_{buffer},
      bitIndex_{0},
      bufferedByte_{*currentBytePtr_}
{}

uint8_t const* BitsReader::currentBytePtr() const
{ return currentBytePtr_; }

uint8_t BitsReader::readBit()
{
    uint8_t bit = bufferedByte_ & 1;
    ++bitIndex_;
    if (bitIndex_ == BITS_IN_BYTE)
    { bufferNextByte(); }
    else
    { bufferedByte_ >>= 1; }
    return bit;
}

uint8_t BitsReader::readBool()
{ return readBit() != 0; }

void BitsReader::bufferNextByte()
{
    ++currentBytePtr_;
    bufferedByte_ = *currentBytePtr_;
    bitIndex_ = 0;
}
