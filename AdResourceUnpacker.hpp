#ifndef ADRESOURCEUNPACKER_HPP
#define ADRESOURCEUNPACKER_HPP

#include "BitsHelper.hpp"
#include <QByteArray>
#include <QString>
#include <cstdint>

class AdResourceUnpacker
{
    static constexpr uint32_t DEFAULT_OUT_BUFFER_SIZE = 0x2000;

public:
    AdResourceUnpacker(uint8_t const* buffer, uint32_t size);

    void setOutBufferSize(uint32_t outBufferSize);
    QByteArray unpack();

private:
    void resetInternalVariables();
    void duplicateWrittenBytes(uint16_t offset, uint16_t bytesNumber);
    void resizeBufferToWrittenData();
    uint8_t readByte();
    uint16_t readTwoBytes();
    uint8_t readControlBit();
    uint8_t readTwoControlBits();
    bool readControlBoolFlag();
    inline void writeByte(uint8_t byte)
    {
        if (outPtr_ >= outBufferEnd_)
        { throw QString("Trying to write beyond output buffer."); }
        *outPtr_ = byte;
        ++outPtr_;
    }

    uint8_t const* inBuffer_;
    uint32_t inBufferSize_;
    uint8_t const* inBufferEnd_;
    uint32_t outBufferSize_{DEFAULT_OUT_BUFFER_SIZE};
    uint8_t const* inPtr_;
    uint8_t controlByte_;
    uint8_t controlByteBitIndex_;
    QByteArray outBuffer_;
    uint8_t* outPtr_;
    uint8_t const* outBufferEnd_;
};

#endif // ADRESOURCEUNPACKER_HPP
