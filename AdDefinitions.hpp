#ifndef ADDEFINITIONS_HPP
#define ADDEFINITIONS_HPP

#include "PsxRamAddress.hpp"
#include <QRect>

enum class GameMode : uint16_t
{
    None = 0,
    InTown = 1,
    InTower = 2,
    TitleScreen = 3
};

struct PortraitData
{
    PsxRamAddress portraitMemoryLoadInfoAddress;
    PsxRamAddress animationAddress;
};

struct MemoryLoadInfo
{
    uint32_t address : 23;
    uint32_t sectorsNumber : 9;
    uint32_t sector;
};

struct GameModeData
{
    PsxRamAddress loopFunctionAddress;
    PsxRamAddress memoryLoadInfoAddress;
};

enum class ResourceType : uint16_t
{
    None = 0,
    VRamLoadablePackedImage = 1,
    Palette = 2,
    PackedImage = 3
};

struct ResourceHeader
{
    ResourceType type;
    uint16_t headerSize;
    uint32_t dataStartOffset;
};

struct Rect
{
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;

    QRect toQRect() const
    { return QRect(x, y, width, height); }
};

struct ResourceType1Header : ResourceHeader
{
    Rect rect;
};

struct PaletteResourcePosition
{
    uint16_t x : 6;
    uint16_t yOffsetFromY448 : 6;
    uint16_t xShiftFlag : 1;

    bool isXShiftFlagSet() const
    { return xShiftFlag != 0; }
};

struct PaletteResourceFlags
{
    uint16_t value;

    bool shouldCopyToVRam() const
    { return (value & 1) == 0; }
};

struct ResourceType2Header : ResourceHeader
{
    PaletteResourcePosition position;
    uint16_t numberOf4BppPalettes;
    PaletteResourceFlags flags;
};

struct ResourceType3Header : ResourceHeader
{
    uint32_t unpackedDataOffset;
};

enum class AnimationFrameType : uint16_t
{
    NoAnimation = 0,
    LastFrame = 1,
    NotLastFrame = 2,
};

struct Animation
{
    uint8_t duration;
    uint8_t padding;
    AnimationFrameType frameType;
    PsxRamAddress graphicAddress;
};

struct Clut
{
    uint16_t x : 6;
    uint16_t y : 9;
};

enum class TexpageBpp : uint16_t
{
    BPP_4 = 0,
    BPP_8 = 1,
    BPP_15 = 2
};

struct Texpage
{
    uint16_t x : 4;
    uint16_t y : 1;
    uint16_t semiTransparency : 2;
    uint16_t bpp : 2;
    uint16_t ditherEnabled : 1;
    uint16_t drawingToDisplayAreaAllowed : 1;
    uint16_t textureDisable : 1;
    uint16_t xFlip : 1;
    uint16_t yFlip : 1;

    TexpageBpp texpageBpp() const
    { return static_cast<TexpageBpp>(bpp); }
};

enum class GraphicFlags : uint8_t
{
    None = 0x0,
    FlipHorizontally = 0x1,
    FlipVertically = 0x2,
    PartOfSeries = 0x40,
    SeriesEnd = 0x80
};

GraphicFlags operator|(GraphicFlags one, GraphicFlags other);
GraphicFlags operator&(GraphicFlags one, GraphicFlags other);
GraphicFlags operator^(GraphicFlags one, GraphicFlags other);

struct Graphic
{
    GraphicFlags flags;
    uint8_t padding;
    int8_t xOffset;
    int8_t yOffset;
    Texpage texpage;
    Clut clut;
    uint8_t xOffsetInTexpage;
    uint8_t yOffsetInTexpage;
    uint8_t width;
    uint8_t height;

    bool hasFlags(GraphicFlags expectedflags) const
    { return (flags & expectedflags) == expectedflags; }
};

#endif // ADDEFINITIONS_HPP
