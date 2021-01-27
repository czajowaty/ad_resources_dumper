#include "BinCdImageReader.hpp"
#include <QFile>

std::unique_ptr<BinCdImageReader> BinCdImageReader::create(
        QString const& filePath)
{
    auto* binFile = new QFile(filePath);
    if (!binFile->open(QIODevice::ReadOnly))
    {
        throw QString("Could not open file %1. %2")
                .arg(filePath)
                .arg(binFile->errorString());
    }
    auto* binCdImageReader = new BinCdImageReader(binFile);
    binFile->setParent(binCdImageReader);
    return std::unique_ptr<BinCdImageReader>(binCdImageReader);
}

BinCdImageReader::BinCdImageReader(QIODevice* binFile)
    : binFile_{binFile}
{
    binFile_->seek(0);
    binFileSize_ = binFile_->bytesAvailable();
}

uint32_t BinCdImageReader::calculateSectorsNumber(uint32_t dataSize)
{
    uint32_t sectorsNumber = dataSize / DATA_IN_SECTOR_SIZE;
    if ((dataSize % DATA_IN_SECTOR_SIZE) > 0)
    { ++sectorsNumber; }
    return sectorsNumber;
}

QByteArray BinCdImageReader::readSector(uint32_t sector)
{ return readSectors(sector, 1); }

QByteArray BinCdImageReader::readSectors(
        uint32_t startSector,
        uint32_t sectorsNumber)
{
    QByteArray sectorData;
    sectorData.resize(DATA_IN_SECTOR_SIZE * sectorsNumber);
    char* sectorDataBuffer = sectorData.data();
    for (
         uint32_t sector = startSector;
         sector < startSector + sectorsNumber;
         ++sector)
    {
        auto readBytes = readSector(sectorDataBuffer, sector);
        sectorDataBuffer += readBytes;
    }
    return sectorData;
}

uint32_t BinCdImageReader::readSector(char* buffer, uint32_t sector)
{
    setFilePositionToSector(sector);
    auto readBytes = binFile_->read(buffer, DATA_IN_SECTOR_SIZE);
    if (readBytes != DATA_IN_SECTOR_SIZE)
    {
        throw QString(
                    "Expected to read %1 bytes for sector 0x%2. Read %3 bytes.")
                .arg(DATA_IN_SECTOR_SIZE)
                .arg(sector, 0, 16)
                .arg(readBytes);
    }
    return DATA_IN_SECTOR_SIZE;
}

void BinCdImageReader::setFilePositionToSector(uint32_t sector)
{
    auto fileOffset = calculateSectorFileOffset(sector);
    fileOffset += DATA_IN_SECTOR_OFFSET;
    if (!binFile_->seek(fileOffset))
    {
        throw QString("Could not set file position to 0x%1. %2")
                .arg(fileOffset, 0, 16)
                .arg(binFile_->errorString());
    }
}

int BinCdImageReader::calculateSectorFileOffset(uint32_t sector)
{ return sector * SECTOR_SIZE; }
