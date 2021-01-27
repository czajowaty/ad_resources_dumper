#ifndef VIRTUALPSXRAM_HPP
#define VIRTUALPSXRAM_HPP

#include "PsxRamAddress.hpp"
#include "PsxRamConst.hpp"
#include <QString>
#include <QVector>
#include <array>

class VirtualPsxRam
{
    using PsxRamBuffer = std::array<uint8_t, PsxRamConst::SIZE>;

public:
    VirtualPsxRam();

    void clear();
    QVector<PsxRamAddress::Region> const& initializedRegions() const;
    uint8_t readByte(PsxRamAddress address) const;
    int8_t readSBbyte(PsxRamAddress address) const;
    uint16_t readWord(PsxRamAddress address) const;
    int16_t readSWord(PsxRamAddress address) const;
    uint32_t readDWord(PsxRamAddress address) const;
    int32_t readSDWord(PsxRamAddress address) const;
    PsxRamAddress readAddress(PsxRamAddress address) const;
    void readRegion(PsxRamAddress::Region const& region, uint8_t* buffer) const;
    void writeByte(uint8_t byte, PsxRamAddress address);
    void writeSByte(int8_t sbyte, PsxRamAddress address);
    void writeWord(uint16_t word, PsxRamAddress address);
    void writeSWord(int16_t sword, PsxRamAddress address);
    void writeDWord(uint32_t dword, PsxRamAddress address);
    void writeSDWord(int32_t sdword, PsxRamAddress address);
    void writeAddress(
            PsxRamAddress addressToWrite,
            PsxRamAddress addressToWriteTo);
    void load(QByteArray const& data, PsxRamAddress address);

private:
    constexpr std::size_t size() const
    { return ram_.size(); }
    template <typename T>
    T read(PsxRamAddress address) const
    {
        constexpr uint32_t const size = sizeof(T);
        PsxRamAddress inPsxRamAddress = PsxRamConst::toNoSegRamAddress(
                    address);
        if (!isInPsxRamRegion(inPsxRamAddress, size))
        { throwReadOutOfBoundsError(address, size); }
        if (!isInitializedRegion({inPsxRamAddress, size}))
        { throwUnitializedReadError(inPsxRamAddress, size); }
        return *reinterpret_cast<T const*>(inBufferPointer(inPsxRamAddress));
    }
    bool isInitializedRegion(PsxRamAddress::Region const& region) const;
    template <typename T>
    void write(T t, PsxRamAddress address)
    { write(reinterpret_cast<uint8_t const*>(t), sizeof(t), address); }
    void write(
            uint8_t const* bytes,
            uint32_t bytesNumber,
            PsxRamAddress address);
    bool isInPsxRamRegion(PsxRamAddress address, uint32_t size) const;
    uint8_t* inBufferPointer(PsxRamAddress address);
    uint8_t const* inBufferPointer(PsxRamAddress address) const;
    void initializeRegion(PsxRamAddress::Region const& region);
    void throwReadOutOfBoundsError(PsxRamAddress address, uint32_t size) const;
    void throwWriteOutOfBoundsError(PsxRamAddress address, uint32_t size) const;
    void throwOutOfBoundsError(
            char const* typeString,
            PsxRamAddress address,
            uint32_t size) const;
    void throwUnitializedReadError(PsxRamAddress address, uint32_t size) const;

    PsxRamBuffer ram_;
    QVector<PsxRamAddress::Region> initializedRegions_;
};

#endif // VIRTUALPSXRAM_HPP
