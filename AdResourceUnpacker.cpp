#include "AdResourceUnpacker.hpp"

AdResourceUnpacker::AdResourceUnpacker(uint8_t const* buffer, uint32_t size)
    : inBuffer_{buffer},
      inBufferSize_{size},
      inBufferEnd_{inBuffer_ + inBufferSize_}
{}

void AdResourceUnpacker::setOutBufferSize(uint32_t outBufferSize)
{ outBufferSize_ = outBufferSize; }

QByteArray AdResourceUnpacker::unpack()
{
    resetInternalVariables();
    while (true)
    {
        while (!readControlBoolFlag())
        { writeByte(readByte()); }

        uint16_t bytesToDuplicateNumber;
        uint16_t startDuplicationFromOffset;
        if (readControlBoolFlag())
        {
            bytesToDuplicateNumber = readTwoControlBits() + 2;
            startDuplicationFromOffset = readByte();
            if (startDuplicationFromOffset == 0)
            { startDuplicationFromOffset = 0x100; }
        }
        else
        {
            uint16_t nextTwoBytes  = readTwoBytes();
            if (nextTwoBytes == 0)
            { break; }
            bytesToDuplicateNumber = (nextTwoBytes & 0xf);
            if (bytesToDuplicateNumber == 0)
            { bytesToDuplicateNumber = readByte() + 1; }
            else
            { bytesToDuplicateNumber += 2; }
            startDuplicationFromOffset = nextTwoBytes >> 4;
        }
        duplicateWrittenBytes(
                    startDuplicationFromOffset,
                    bytesToDuplicateNumber);
    }
    resizeBufferToWrittenData();
    return outBuffer_;
}

void AdResourceUnpacker::resetInternalVariables()
{
    controlByteBitIndex_ = BITS_IN_BYTE;
    inPtr_ = inBuffer_;
    outBuffer_.resize(outBufferSize_);
    outPtr_ = reinterpret_cast<uint8_t*>(outBuffer_.data());
    outBufferEnd_ = outPtr_ + outBuffer_.size();
}

void AdResourceUnpacker::duplicateWrittenBytes(
        uint16_t offset,
        uint16_t bytesNumber)
{
    auto const* src = outPtr_ - offset;
    if (src < reinterpret_cast<decltype(src)>(outBuffer_.data()))
    { throw QString("Trying to read beyond output buffer."); }
    for (uint16_t i = 0; i < bytesNumber; ++i, ++src)
    { writeByte(*src); }
}

void AdResourceUnpacker::resizeBufferToWrittenData()
{
    auto outBufferSize =
            outPtr_ - reinterpret_cast<uint8_t const*>(outBuffer_.data());
    outBuffer_.resize(outBufferSize);
}

uint8_t AdResourceUnpacker::readByte()
{
    if (inPtr_ >= inBufferEnd_)
    { throw QString("Trying to read beyond input buffer."); }
    uint8_t nextByte = *inPtr_;
    ++inPtr_;
    return nextByte;
}

uint16_t AdResourceUnpacker::readTwoBytes()
{
    uint16_t nextTwoBytes = readByte();
    nextTwoBytes <<= 8;
    nextTwoBytes |= readByte();
    return nextTwoBytes;
}

uint8_t AdResourceUnpacker::readControlBit()
{
    if (controlByteBitIndex_ >= BITS_IN_BYTE)
    {
        controlByte_ = readByte();
        controlByteBitIndex_ = 0;
    }
    uint8_t controlBit = controlByte_ & 1;
    controlByte_ >>= 1;
    ++controlByteBitIndex_;
    return controlBit;
}

uint8_t AdResourceUnpacker::readTwoControlBits()
{ return (readControlBit() << 1) | readControlBit(); }

bool AdResourceUnpacker::readControlBoolFlag()
{ return readControlBit() != 0; }
