#ifndef VIRTUALPSXVRAM_HPP
#define VIRTUALPSXVRAM_HPP

#include "AdDefinitions.hpp"
#include "PsxVRamConst.hpp"
#include <QImage>
#include <array>

class VirtualPsxVRam
{
    template <std::size_t SIZE>
    struct Palette
    {
        std::array<QRgb, SIZE> data;

        static constexpr uint16_t size()
        { return SIZE; }
        static constexpr uint16_t rawDataSize()
        { return SIZE * PsxVRamConst::PIXEL_SIZE; }
    };

public:
    using Palette4Bpp = Palette<valuesInBits(4)>;
    using Palette8Bpp = Palette<valuesInBits(8)>;

private:
    using ColorMapping = std::array<uint8_t, valuesInBits(5)>;
    static constexpr ColorMapping COLOR_MAPPING = {
        0, 23, 47, 63, 79, 95, 103, 111,
        119, 127, 135, 143, 151, 159, 167, 175,
        183, 191, 199, 207, 211, 215, 219, 223,
        227, 231, 235, 239, 243, 247, 251, 255
    };
    using PsxVRamBuffer = std::array<uint8_t, PsxVRamConst::SIZE>;

    struct Pixel16Bpp
    {
        uint8_t r;
        uint8_t g;
        uint8_t b;
        bool semiTransparent;

        QRgb toRgb() const
        { return qRgb(r, g, b); }
        QRgb toRgba() const
        {
            if (r != 0 || g != 0 || b != 0 || semiTransparent)
            { return qRgba(r, g, b, 0xff); }
            else
            { return qRgba(0, 0, 0, 0); }
        }
    };

    struct Pixel8Bpp
    {
        uint8_t paletteIndexLeft;
        uint8_t paletteIndexRight;
    };

    struct Pixel4Bpp
    {
        uint8_t paletteIndexLeft;
        uint8_t paletteIndexMiddleLeft;
        uint8_t paletteIndexMiddleRight;
        uint8_t paletteIndexRight;
    };

public:
    static QSize const TEXTURE_PAGE_SIZE;

    VirtualPsxVRam();

    constexpr std::size_t size() const
    { return vram_.size(); }

    void clear();
    QVector<QRect> const& initializedRects() const;
    QRect const& initializedBoundingRect() const;
    QImage asImage() const;
    QImage read16BppTexture(Graphic const& graphic) const;
    QImage read16BppTexture(QRect const& rect) const;
    QImage read8BppTexture(
            Graphic const& graphic,
            Palette8Bpp const* palette) const;
    QImage read8BppTexture(QRect const& rect, Palette8Bpp const* palette) const;
    QImage read4BppTexture(
            Graphic const& graphic,
            Palette4Bpp const* palette) const;
    QImage read4BppTexture(QRect const& rect, Palette4Bpp const* palette) const;
    void read4BppPalette(Clut const& clut, Palette4Bpp& palette) const;
    void read4BppPalette(QPoint const& point, Palette4Bpp& palette) const;
    void read8BppPalette(Clut const& clut, Palette8Bpp& palette) const;
    void read8BppPalette(QPoint const& point, Palette8Bpp& palette) const;
    void load(QByteArray const& data, QRect const& rect);
    void load(uint8_t const* data, uint32_t dataSize, QRect const& rect);
    static QRect calculateVRamRect(Graphic const& graphic);

private:
    template <std::size_t PALETTE_SIZE>
    QVector<QRgb> prepareColorTable(Palette<PALETTE_SIZE> const* palette) const
    {
        QRgb const* paletteBegin = palette->data.cbegin();
        return QVector<QRgb>(paletteBegin, paletteBegin + PALETTE_SIZE);
    }
    QImage readTexture(
            Graphic const& graphic,
            TexpageBpp expectedBpp,
            std::function<QImage(Graphic const&)> textureReader) const;
    template <std::size_t PALETTE_SIZE>
    void readPalette(
            QPoint const& vramPoint,
            Palette<PALETTE_SIZE>& palette) const
    {
        QRect paletteRect(vramPoint, QSize(palette.rawDataSize(), 1));
        if (!isRectInitialized(paletteRect))
        {
            throwUninitializedRectError(
                        paletteRect,
                        QString("%1 colors palette").arg(PALETTE_SIZE));
        }
        uint8_t const* vramPixelAddress =
                pixelAddress(vramPoint.x(), vramPoint.y());
        for (uint32_t index = 0; index < PALETTE_SIZE; ++index)
        {
            palette.data[index] = read16BppPixel(vramPixelAddress).toRgba();
            vramPixelAddress += PsxVRamConst::PIXEL_SIZE;
        }
    }
    void assertTextureDepth(Texpage const& texpage, TexpageBpp expected) const;
    QPoint clutToVRamPoint(Clut const& clut) const;
    uint8_t const* pixelAddress(int x, int y) const;
    uint8_t const* scanLine(int y) const;
    uint8_t* pixelAddress(int x, int y);
    uint8_t* scanLine(int y);
    Pixel16Bpp read16BppPixel(uint8_t const* pixelAddress) const;
    void appendInitializedRect(QRect const& rect);
    bool isRectInitialized(QRect const& rect) const;
    bool isPointInitialized(QPoint const& point) const;
    void throwUninitializedRectError(
            QRect const& rect,
            QString const& readType) const;
    static uint8_t inTextureXShift(TexpageBpp bpp);
    static QPoint calculateTextureVRamPoint(Graphic const& graphic);

    PsxVRamBuffer vram_;
    QVector<QRect> initializedRects_;
    QRect initializedBoundingRect_;
};

#endif // VIRTUALPSXVRAM_HPP
