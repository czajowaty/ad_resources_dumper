#include "VirtualPsxVRam.hpp"

#include <cstring>

constexpr VirtualPsxVRam::ColorMapping VirtualPsxVRam::COLOR_MAPPING;

VirtualPsxVRam::VirtualPsxVRam()
{ clear(); }

void VirtualPsxVRam::clear()
{
    vram_.fill(0);
    initializedRects_.clear();
}

QVector<QRect> const& VirtualPsxVRam::initializedRects() const
{ return initializedRects_; }

QRect const& VirtualPsxVRam::initializedBoundingRect() const
{ return initializedBoundingRect_; }

QImage VirtualPsxVRam::asImage() const
{
    QImage image(
                PsxVRamConst::PIXELS_PER_LINE,
                PsxVRamConst::HEIGHT,
                QImage::Format_ARGB32);
    int x = 0;
    int y = 0;
    QRgb* imagePixelAddress = reinterpret_cast<QRgb*>(image.scanLine(y));
    for (
         auto it = vram_.cbegin();
         it != vram_.cend();
         it += PsxVRamConst::PIXEL_SIZE)
    {
        *imagePixelAddress = read16BppPixel(it).toRgb();
        ++x;
        if (x < image.width())
        { ++imagePixelAddress; }
        else
        {
            x = 0;
            ++y;
            imagePixelAddress = reinterpret_cast<QRgb*>(image.scanLine(y));
        }
    }
    return image;
}

QImage VirtualPsxVRam::read16BppTexture(Graphic const& graphic) const
{
    return readTexture(
                graphic,
                TexpageBpp::BPP_15,
                [this](Graphic const& graphic) {
        return read16BppTexture(calculateVRamRect(graphic));
    });
}

QImage VirtualPsxVRam::read16BppTexture(QRect const& rect) const
{
    if (!isRectInitialized(rect))
    { throwUninitializedRectError(rect, "16 Bpp texture"); }
    QImage image(rect.width(), rect.height(), QImage::Format_ARGB32);
    for (int y = 0; y < image.height(); ++y)
    {
        QRgb* imagePixelAddress = reinterpret_cast<QRgb*>(image.scanLine(y));
        uint8_t const* vramPixelAddress = pixelAddress(rect.x(), rect.y() + y);
        for (int x = 0; x < image.width(); ++x)
        {
            *imagePixelAddress = read16BppPixel(vramPixelAddress).toRgba();
            ++imagePixelAddress;
            vramPixelAddress += PsxVRamConst::PIXEL_SIZE;
        }
    }
    return image;
}

QImage VirtualPsxVRam::read8BppTexture(
        Graphic const& graphic,
        Palette8Bpp const* palette) const
{
    return readTexture(
                graphic,
                TexpageBpp::BPP_8,
                [this, palette](Graphic const& graphic) {
        return read8BppTexture(calculateVRamRect(graphic), palette);
    });
}

QImage VirtualPsxVRam::read8BppTexture(
        QRect const& rect,
        Palette8Bpp const* palette) const
{
    if (!isRectInitialized(rect))
    { throwUninitializedRectError(rect, "8 Bpp texture"); }
    QImage image(rect.width() << 1, rect.height(), QImage::Format_Indexed8);
    image.setColorTable(prepareColorTable(palette));
    for (int y = 0; y < image.height(); ++y)
    {
        std::memcpy(
                    image.scanLine(y),
                    pixelAddress(rect.x(), rect.y() + y),
                    image.width());
    }
    return image;
}

QImage VirtualPsxVRam::read4BppTexture(
        Graphic const& graphic,
        Palette4Bpp const* palette) const
{
    return readTexture(
                graphic,
                TexpageBpp::BPP_4,
                [this, palette](Graphic const& graphic) {
        return read4BppTexture(calculateVRamRect(graphic), palette);
    });
}

QImage VirtualPsxVRam::read4BppTexture(
        QRect const& rect,
        Palette4Bpp const* palette) const
{
    if (!isRectInitialized(rect))
    { throwUninitializedRectError(rect, "4 Bpp texture"); }
    QImage image(rect.width() << 2, rect.height(), QImage::Format_Indexed8);
    image.setColorTable(prepareColorTable(palette));
    for (int y = 0; y < rect.height(); ++y)
    {
        uint8_t* imagePixelAddress = image.scanLine(y);
        uint8_t const* vramPixelAddress = pixelAddress(rect.x(), rect.y() + y);
        for (int x = 0; x < rect.width(); ++x)
        {
            uint8_t vramPixel1 = *vramPixelAddress;
            uint8_t vramPixel2 = *(vramPixelAddress + 1);
            *imagePixelAddress = vramPixel1 & 0xf;
            *(imagePixelAddress + 1) = (vramPixel1 & 0xf0) >> 4;
            *(imagePixelAddress + 2) = vramPixel2 & 0xf;
            *(imagePixelAddress + 3) = (vramPixel2 & 0xf0) >> 4;
            imagePixelAddress += 4;
            vramPixelAddress += PsxVRamConst::PIXEL_SIZE;
        }
    }
    return image;
}

QRect VirtualPsxVRam::calculateVRamRect(Graphic const& graphic) const
{
    auto xShift = inTextureXShift(graphic.texpage.texpageBpp());
    return QRect(
        calculateTextureVRamPoint(graphic),
        QSize(graphic.width >> xShift, graphic.height));
}

QImage VirtualPsxVRam::readTexture(
        Graphic const& graphic,
        TexpageBpp expectedBpp,
        std::function<QImage(Graphic const&)> textureReader) const
{
    assertTextureDepth(graphic.texpage, expectedBpp);
    auto texture = textureReader(graphic);
    QSize expectedTextureSize(graphic.width, graphic.height);
    return (texture.size() == expectedTextureSize) ?
                texture :
                texture.copy(QRect(QPoint(0, 0), expectedTextureSize));
}

void VirtualPsxVRam::assertTextureDepth(
        Texpage const& texpage,
        TexpageBpp expected) const
{
    if (texpage.texpageBpp() != expected)
    {
        throw QString("Expected Bpp %1 != bpp %2.")
                .arg(static_cast<uint16_t>(expected))
                .arg(static_cast<uint16_t>(texpage.texpageBpp()));
    }
}

QPoint VirtualPsxVRam::calculateTextureVRamPoint(Graphic const& graphic) const
{
    auto const& texpage = graphic.texpage;
    QPoint point(
                texpage.x << PsxVRamConst::TEXTURE_PAGE_X_SHIFT,
                texpage.y << PsxVRamConst::TEXTURE_PAGE_Y_SHIFT);
    point.rx() +=
            graphic.xOffsetInTexpage >> inTextureXShift(texpage.texpageBpp());
    point.ry() += graphic.yOffsetInTexpage;
    return point;
}

uint8_t VirtualPsxVRam::inTextureXShift(TexpageBpp bpp) const
{
    if (bpp == TexpageBpp::BPP_4)
    { return 2; }
    else if (bpp == TexpageBpp::BPP_8)
    { return 1; }
    else
    { return 0; }
}

void VirtualPsxVRam::read4BppPalette(
        Clut const& clut,
        Palette4Bpp& palette) const
{ return read4BppPalette(clutToVRamPoint(clut), palette); }

void VirtualPsxVRam::read4BppPalette(
        QPoint const& point,
        Palette4Bpp& palette) const
{ return readPalette(point, palette); }

void VirtualPsxVRam::read8BppPalette(
        Clut const& clut,
        Palette8Bpp& palette) const
{ return read8BppPalette(clutToVRamPoint(clut), palette); }

void VirtualPsxVRam::read8BppPalette(
        QPoint const& point,
        Palette8Bpp& palette) const
{ return readPalette(point, palette); }

QPoint VirtualPsxVRam::clutToVRamPoint(Clut const& clut) const
{ return QPoint(clut.x << PsxVRamConst::CLUT_X_SHIFT, clut.y); }

uint8_t const* VirtualPsxVRam::pixelAddress(int x, int y) const
{ return scanLine(y) + (x << PsxVRamConst::PIXEL_DEPTH); }

uint8_t const* VirtualPsxVRam::scanLine(int y) const
{ return vram_.cbegin() + (y << PsxVRamConst::WIDTH_DEPTH); }

uint8_t* VirtualPsxVRam::pixelAddress(int x, int y)
{ return scanLine(y) + (x << PsxVRamConst::PIXEL_DEPTH); }

uint8_t* VirtualPsxVRam::scanLine(int y)
{ return vram_.begin() + (y << PsxVRamConst::WIDTH_DEPTH); }

VirtualPsxVRam::Pixel16Bpp VirtualPsxVRam::read16BppPixel(
        uint8_t const* pixelAddress) const
{
    Pixel16Bpp pixel;
    uint8_t byte1 = *pixelAddress;
    uint8_t byte2 = *(pixelAddress + 1);

    pixel.r = COLOR_MAPPING[(byte1 & 0x1f)];
    pixel.g = COLOR_MAPPING[(byte1 >> 5) | ((byte2 & 0x03) << 3)];
    pixel.b = COLOR_MAPPING[(byte2 >> 2) & 0x1f];
    pixel.semiTransparent = (byte2 & 0x80) == 0x80;
    return pixel;
}

void VirtualPsxVRam::load(QByteArray const& data, QRect const& rect)
{ load(reinterpret_cast<uint8_t const*>(data.constData()), data.size(), rect); }

void VirtualPsxVRam::load(
        uint8_t const* data,
        uint32_t dataSize,
        QRect const& rect)
{
    if (rect.isEmpty())
    {
        throw QString("Invalid rect (%1, %2)(%3, %4).")
                .arg(rect.left())
                .arg(rect.top())
                .arg(rect.right())
                .arg(rect.bottom());
    }
    if (
            rect.x() < 0 || rect.right() >= PsxVRamConst::PIXELS_PER_LINE ||
            rect.y() < 0 || rect.bottom() >= PsxVRamConst::HEIGHT)
    {
        throw QString("Rect (%1, %2)(%3, %4) is out of VRAM bounds.")
                .arg(rect.left())
                .arg(rect.top())
                .arg(rect.right())
                .arg(rect.bottom());
    }
    auto scanLineDataSize = rect.width() * PsxVRamConst::PIXEL_SIZE;
    uint32_t expectedDataSize = scanLineDataSize * rect.height();
    if (dataSize != expectedDataSize)
    {
        throw QString(
                    "With rect (%1, %2)(%3, %4) expected data size is 0x%5. "
                    "Provided data size 0x%6.")
                .arg(rect.left())
                .arg(rect.top())
                .arg(rect.right())
                .arg(rect.bottom())
                .arg(expectedDataSize, 0, 16)
                .arg(dataSize, 0, 16);
    }
    auto* dataStartIt = data;
    for (int y = rect.y(); y <= rect.bottom(); ++y)
    {
        auto* dataEndIt = dataStartIt + scanLineDataSize;
        auto* vramPixel = pixelAddress(rect.x(), y);
        std::copy(dataStartIt, dataEndIt, vramPixel);
        dataStartIt = dataEndIt;
    }
    appendInitializedRect(rect);
}

void VirtualPsxVRam::appendInitializedRect(QRect const& rect)
{
    initializedBoundingRect_ = initializedBoundingRect_.united(rect);
    for (int i = 0; i < initializedRects_.size(); ++i)
    {
        auto& initializedRect = initializedRects_[i];
        if (initializedRect.contains(rect))
        { return; }
        // TODO: add combining rects when adjacent
    }
    initializedRects_.append(rect);
}

bool VirtualPsxVRam::isRectInitialized(QRect const& rect) const
{
    // This function is not fully correct. It will all only tell wheter
    // all rectangle corner points are initialized.
    if (!initializedBoundingRect_.contains(rect))
    { return false; }
    if (!isPointInitialized(rect.topLeft()))
    { return false; }
    if (!isPointInitialized(rect.topRight()))
    { return false; }
    if (!isPointInitialized(rect.bottomLeft()))
    { return false; }
    if (!isPointInitialized(rect.bottomLeft()))
    { return false; }
    return true;
}

bool VirtualPsxVRam::isPointInitialized(QPoint const& point) const
{
    for (auto const& initializedRect : initializedRects_)
    {
        if (initializedRect.contains(point))
        { return true; }
    }
    return false;
}

void VirtualPsxVRam::throwUninitializedRectError(
        QRect const& rect,
        QString const& readType) const
{
    throw QString("Trying to read %1 from uninitialized rect (%2, %3)(%4, %5).")
            .arg(readType)
            .arg(rect.left())
            .arg(rect.top())
            .arg(rect.right())
            .arg(rect.bottom());
}
