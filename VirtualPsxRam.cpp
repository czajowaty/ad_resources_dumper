#include "VirtualPsxRam.hpp"

#include <algorithm>
#include <cstring>

VirtualPsxRam::VirtualPsxRam()
{ clear(); }

void VirtualPsxRam::clear()
{
    ram_.fill(0);
    initializedRegions_.clear();
}

QVector<PsxRamAddress::Region> const& VirtualPsxRam::initializedRegions() const
{ return initializedRegions_; }

uint8_t VirtualPsxRam::readByte(PsxRamAddress address) const
{ return read<uint8_t>(address); }

int8_t VirtualPsxRam::readSBbyte(PsxRamAddress address) const
{ return read<int8_t>(address); }

uint16_t VirtualPsxRam::readWord(PsxRamAddress address) const
{ return read<uint16_t>(address); }

int16_t VirtualPsxRam::readSWord(PsxRamAddress address) const
{ return read<int16_t>(address); }

uint32_t VirtualPsxRam::readDWord(PsxRamAddress address) const
{ return read<uint32_t>(address); }

int32_t VirtualPsxRam::readSDWord(PsxRamAddress address) const
{ return read<int32_t>(address); }

PsxRamAddress VirtualPsxRam::readAddress(PsxRamAddress address) const
{ return read<PsxRamAddress::Raw>(address); }

void VirtualPsxRam::readRegion(
        PsxRamAddress::Region const& region,
        uint8_t* buffer) const
{
    PsxRamAddress inPsxRamAddress = PsxRamConst::toNoSegRamAddress(
                region.address);
    if (!isInPsxRamRegion(inPsxRamAddress, region.size))
    { throwReadOutOfBoundsError(region.address, region.size); }
    if (!isInitializedRegion({inPsxRamAddress, region.size}))
    { throwUnitializedReadError(region.address, region.size); }
    std::memcpy(buffer, inBufferPointer(inPsxRamAddress), region.size);
}

bool VirtualPsxRam::isInitializedRegion(
        PsxRamAddress::Region const& region) const
{
    for (auto const& initializedRegion : initializedRegions_)
    {
        if (initializedRegion.doesContain(region))
        { return true; }
    }
    return false;
}

void VirtualPsxRam::writeByte(uint8_t byte, PsxRamAddress address)
{ write(byte, address); }

void VirtualPsxRam::writeSByte(int8_t sbyte, PsxRamAddress address)
{ write(sbyte, address); }

void VirtualPsxRam::writeWord(uint16_t word, PsxRamAddress address)
{ write(word, address); }

void VirtualPsxRam::writeSWord(int16_t sword, PsxRamAddress address)
{ write(sword, address); }

void VirtualPsxRam::writeDWord(uint32_t dword, PsxRamAddress address)
{ write(dword, address); }

void VirtualPsxRam::writeSDWord(int32_t sdword, PsxRamAddress address)
{ write(sdword, address); }

void VirtualPsxRam::writeAddress(
        PsxRamAddress addressToWrite,
        PsxRamAddress addressToWriteTo)
{ write(PsxRamConst::toKSeg0RamAddress(addressToWrite), addressToWriteTo); }

void VirtualPsxRam::load(QByteArray const& data, PsxRamAddress address)
{
    write(
                reinterpret_cast<uint8_t const*>(data.constData()),
                data.size(),
                address);
}

void VirtualPsxRam::write(
        uint8_t const* bytes,
        uint32_t bytesNumber,
        PsxRamAddress address)
{
    PsxRamAddress inPsxRamAddress = PsxRamConst::toNoSegRamAddress(address);
    if (!isInPsxRamRegion(inPsxRamAddress, bytesNumber))
    { throwWriteOutOfBoundsError(address, bytesNumber); }
    std::memcpy(inBufferPointer(inPsxRamAddress), bytes, bytesNumber);
    initializeRegion({inPsxRamAddress, bytesNumber});
}

bool VirtualPsxRam::isInPsxRamRegion(
        PsxRamAddress address,
        uint32_t size) const
{
    if (size > 0)
    { address.addOffset(size - 1); }
    return PsxRamConst::isInPsxRamAddress(address);
}

uint8_t* VirtualPsxRam::inBufferPointer(PsxRamAddress address)
{ return ram_.begin() + address.raw(); }

uint8_t const* VirtualPsxRam::inBufferPointer(PsxRamAddress address) const
{ return ram_.cbegin() + address.raw(); }

void VirtualPsxRam::initializeRegion(PsxRamAddress::Region const& region)
{
    for (int i = 0; i < initializedRegions_.size(); ++i)
    {
        auto& initializedRegion = initializedRegions_[i];
        if (initializedRegion.doesContain(region))
        { return; }
        if (region.doesContain(initializedRegion))
        {
            initializedRegion = region;
            return;
        }
        if (
                initializedRegion.doesIntersect(region) ||
                initializedRegion.isAdjacent(region))
        {
            auto startAddress =
                    initializedRegion.address <= region.address ?
                        initializedRegion.address :
                        region.address;
            auto endAddress = initializedRegion.end() >= region.end() ?
                        initializedRegion.end() :
                        region.end();
            uint32_t size =
                    endAddress > startAddress ?
                        endAddress - startAddress + 1 :
                        0;
            initializedRegion = {startAddress, size};
            return;
        }
    }
    initializedRegions_.append(region);
}

void VirtualPsxRam::throwReadOutOfBoundsError(
        PsxRamAddress address,
        uint32_t size) const
{ throwOutOfBoundsError("Read", address, size); }

void VirtualPsxRam::throwWriteOutOfBoundsError(
        PsxRamAddress address,
        uint32_t size) const
{ throwOutOfBoundsError("Write", address, size); }

void VirtualPsxRam::throwOutOfBoundsError(
        char const* typeString,
        PsxRamAddress address,
        uint32_t size) const
{
    throw QString("%1 out of bounds error (address: 0x%2, size: 0x%3).")
            .arg(typeString)
            .arg(address.raw(), 0, 16)
            .arg(size, 0, 16);
}

void VirtualPsxRam::throwUnitializedReadError(
        PsxRamAddress address,
        uint32_t size) const
{
    throw QString("Read from uninitialized region (address: 0x%1, size: 0x%2).")
            .arg(address.raw(), 0, 16)
            .arg(size, 0, 16);
}
