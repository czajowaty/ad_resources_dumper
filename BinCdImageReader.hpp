#ifndef BINCDIMAGEREADER_HPP
#define BINCDIMAGEREADER_HPP

#include <QIODevice>
#include <memory>

class BinCdImageReader : public QObject
{
    static constexpr uint32_t SECTOR_SIZE = 0x930;
    static constexpr uint32_t DATA_IN_SECTOR_OFFSET = 0x18;

public:
    static constexpr uint32_t DATA_IN_SECTOR_SIZE = 0x800;

    static std::unique_ptr<BinCdImageReader> create(QString const& filePath);

    static uint32_t calculateSectorsNumber(uint32_t dataSize);
    QByteArray readSector(uint32_t sector);
    QByteArray readSectors(uint32_t startSector, uint32_t sectorsNumber);

private:
    BinCdImageReader(QIODevice* binFile);
    BinCdImageReader(BinCdImageReader const&) = delete;

    void setFilePositionToSector(uint32_t sector);
    int calculateSectorFileOffset(uint32_t sector);
    uint32_t readSector(char* buffer, uint32_t sector);

    QIODevice* binFile_;
    uint32_t binFileSize_;
};

#endif // BINCDIMAGEREADER_HPP
