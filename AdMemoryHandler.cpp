#include "AdMemoryHandler.hpp"
#include "AdResourcesIterator.hpp"
#include "AdResourceUnpacker.hpp"
#include <QPainter>

const QPoint AdMemoryHandler::PORTRAIT_POSITION(0x54, 0x8d);
constexpr SpeakerInfo AdMemoryHandler::INVALID_SPEAKER_INFO;

AdMemoryHandler::AdMemoryHandler()
    : ram_{std::make_unique<VirtualPsxRam>()},
      vram_{std::make_unique<VirtualPsxVRam>()}
{}

void AdMemoryHandler::loadCdImage(QString const& cdImagePath)
{
    adCdImageReader_ = BinCdImageReader::create(cdImagePath);
    ram_->clear();
    vram_->clear();
    loadSlusTextSection();
    loadTownResources();
}

void AdMemoryHandler::loadSlusTextSection()
{
    static constexpr uint32_t SLUS_TEXT_SECTION_START_SECTOR = 25;
    static constexpr uint32_t SLUS_TEXT_SECTION_SIZE = 0x54800;
    static constexpr PsxRamAddress::Raw SLUS_TEXT_SECTION_LOAD_ADDRESS =
            0x8002d000;
    ram_->load(
                adCdImageReader_->readSectors(
                    SLUS_TEXT_SECTION_START_SECTOR,
                    BinCdImageReader::calculateSectorsNumber(
                        SLUS_TEXT_SECTION_SIZE)),
                SLUS_TEXT_SECTION_LOAD_ADDRESS);
}

void AdMemoryHandler::loadTownResources()
{
    loadGameModeResources(GameMode::InTown);
    loadTownVRamResources();
}

void AdMemoryHandler::loadGameModeResources(GameMode gameMode)
{
    auto gameModeData = readGameModeData(gameMode);
    if (gameModeData.memoryLoadInfoAddress.isNull())
    { return; }
    auto sectorsNumber = readGameModeDataSectorsNumber(gameMode);
    MemoryLoadInfo memoryLoadInfo;
    ram_->readRegion(
                { gameModeData.memoryLoadInfoAddress, sizeof(MemoryLoadInfo)},
                reinterpret_cast<uint8_t*>(&memoryLoadInfo));
    PsxRamAddress memoryLoadInfoAddress = memoryLoadInfo.address;
    if (memoryLoadInfoAddress.isNull())
    { throw QString("Address where to load resources is null."); }
    memoryLoadInfoAddress |= PsxRamConst::KSEG0_ADDRESS;
    sectorsNumber &= ~0x1ff;
    sectorsNumber |= memoryLoadInfo.sectorsNumber;
    ram_->load(
                adCdImageReader_->readSectors(
                    memoryLoadInfo.sector,
                    sectorsNumber),
                memoryLoadInfoAddress);
}

GameModeData AdMemoryHandler::readGameModeData(GameMode gameMode) const
{
    PsxRamAddress gameModeDataAddress(0x8006ce44);
    gameModeDataAddress += gameModeToIndex(gameMode) * sizeof(GameModeData);
    GameModeData gameModeData;
    ram_->readRegion(
                { gameModeDataAddress, sizeof(GameModeData) },
                reinterpret_cast<uint8_t*>(&gameModeData));
    return gameModeData;
}

uint32_t AdMemoryHandler::readGameModeDataSectorsNumber(GameMode gameMode)
{
    PsxRamAddress gameModeDataSectorsNumberAddress = 0x8006ce6c;
    gameModeDataSectorsNumberAddress +=
            gameModeToIndex(gameMode) * sizeof(uint32_t);
    return ram_->readDWord(gameModeDataSectorsNumberAddress);
}

void AdMemoryHandler::loadTownVRamResources()
{
    MemoryLoadInfo townResourcesMemoryLoadInfo;
    ram_->readRegion(
                {0x80080ea0, sizeof(townResourcesMemoryLoadInfo)},
                reinterpret_cast<uint8_t*>(&townResourcesMemoryLoadInfo));
    auto townResourcesData = adCdImageReader_->readSectors(
                townResourcesMemoryLoadInfo.sector,
                townResourcesMemoryLoadInfo.sectorsNumber);
    AdResourcesIterator resourcesIterator(townResourcesData);
    while (resourcesIterator.hasNext())
    {
        auto nextResourceDescriptor = resourcesIterator.next();
        auto const* resourceHeader = nextResourceDescriptor.header;
        if (resourceHeader->type == ResourceType::VRamLoadablePackedImage)
        {
            loadVRamLoadablePackedImage(
                        reinterpret_cast<ResourceType1Header const*>(
                            resourceHeader),
                        nextResourceDescriptor.data,
                        nextResourceDescriptor.size);
        }
        else if (resourceHeader->type == ResourceType::Palette)
        {
            loadVRamPalette(
                        reinterpret_cast<ResourceType2Header const*>(
                            resourceHeader),
                        nextResourceDescriptor.data);
        }
        else
        {
            throw QString("Unexpected town resource type %1.")
                    .arg(static_cast<uint16_t>(resourceHeader->type));
        }
    }
}

void AdMemoryHandler::loadVRamLoadablePackedImage(
        ResourceType1Header const* resourceHeader,
        uint8_t const* resourceData,
        uint32_t maxSize)
{
    auto resourceTexture = unpackResourceTexture(resourceData, maxSize);
    vram_->load(resourceTexture, resourceHeader->rect.toQRect());
}

QByteArray AdMemoryHandler::unpackResourceTexture(
        uint8_t const* resourceData,
        uint32_t maxSize)
{
    AdResourceUnpacker adResourceUnpacker(resourceData, maxSize);
    uint32_t outBufferSize =
            (maxSize / BinCdImageReader::DATA_IN_SECTOR_SIZE) *
            BinCdImageReader::DATA_IN_SECTOR_SIZE;
    if (maxSize % BinCdImageReader::DATA_IN_SECTOR_SIZE != 0)
    { outBufferSize += BinCdImageReader::DATA_IN_SECTOR_SIZE; }
    outBufferSize *= 2;
    adResourceUnpacker.setOutBufferSize(outBufferSize);
    return adResourceUnpacker.unpack();
}

void AdMemoryHandler::loadVRamPalette(
        ResourceType2Header const* resourceHeader,
        uint8_t const* resourceData)
{
    if (!resourceHeader->flags.shouldCopyToVRam())
    { return; }
    using Palette = VirtualPsxVRam::Palette4Bpp;
    uint16_t palette4BppRawDataSize = Palette::rawDataSize();
    uint16_t palette4BppSize = Palette::size();
    QRect paletteRect(
                resourceHeader->position.x * palette4BppRawDataSize,
                resourceHeader->position.yOffsetFromY448 + 0x1c0,
                resourceHeader->numberOf4BppPalettes * palette4BppSize,
                1);
    vram_->load(
                resourceData,
                resourceHeader->numberOf4BppPalettes * palette4BppRawDataSize,
                paletteRect);
}

CharacterPortraitsData AdMemoryHandler::readCharacterPortraitsData(
        AdSpeakerId speakerId)
{
    auto const& speakerInfo = findSpeakerInfo(speakerId);
    if (!speakerInfo.isValid())
    { return {}; }
    auto speakerPortraitIndex = readSpeakerPortraitIndex(speakerId);
    if (speakerPortraitIndex == INVALID_SPEAKER_PORTRAIT_INDEX)
    { return {}; }
    PsxRamAddress portraitDataTableAddressAddress =
            0x8006b1a8 + speakerPortraitIndex * sizeof(PsxRamAddress);
    auto portraitDataTableAddress =
            ram_->readAddress(portraitDataTableAddressAddress);
    if (portraitDataTableAddress.isNull())
    { return {}; }
    CharacterPortraitsData characterPortraitsData;
    PsxRamAddress portraitDataAddress = portraitDataTableAddress;
    for (
         uint32_t variantIndex = 0;
         variantIndex < speakerInfo.variantsNumber;
         ++variantIndex)
    {
        CharacterPortraitData characterPortraitData;
        auto& portraitData = characterPortraitData.portraitData;
        ram().readRegion(
                    {portraitDataAddress, sizeof(portraitData)},
                    reinterpret_cast<uint8_t*>(&portraitData));
        if (portraitData.portraitMemoryLoadInfoAddress.isNull())
        { break; }
        characterPortraitData.portraitDataAddress = portraitDataAddress;
        characterPortraitsData.append(characterPortraitData);
        portraitDataAddress += sizeof(PortraitData);
    }
    return characterPortraitsData;
}

uint8_t AdMemoryHandler::readSpeakerPortraitIndex(AdSpeakerId speakerId)
{
    static constexpr uint8_t SPEAKERS_WITH_PORTRAIT_NUMBER = 18;
    PsxRamAddress speakersWithPortraitsAddress = 0x8006b1ec;
    for (uint8_t index = 0; index < SPEAKERS_WITH_PORTRAIT_NUMBER; ++index)
    {
        auto portraitSpeakerId =
                ram().readByte(speakersWithPortraitsAddress.raw() + index);
        if (speakerId == static_cast<AdSpeakerId>(portraitSpeakerId))
        { return index; }
    }
    return INVALID_SPEAKER_PORTRAIT_INDEX;
}

SpeakerInfo const& AdMemoryHandler::findSpeakerInfo(AdSpeakerId speakerId) const
{
    for (auto const& speakerInfo : SPEAKERS_INFO)
    {
        if (speakerInfo.speakerId == speakerId)
        { return speakerInfo; }
    }
    return INVALID_SPEAKER_INFO;
}

CharacterPortraitResource AdMemoryHandler::loadCharacterPortrait(
        PortraitData portraitData)
{
    CharacterPortraitResource characterPortraitResource;
    MemoryLoadInfo& memoryLoadInfo =
            characterPortraitResource.resourcesMemoryLoadInfo;
    ram().readRegion(
                {
                    portraitData.portraitMemoryLoadInfoAddress,
                    sizeof(memoryLoadInfo)
                },
                reinterpret_cast<uint8_t*>(&memoryLoadInfo));
    loadPortraitResourceIntoVRam(memoryLoadInfo);
    characterPortraitResource.animationFrames =
            readAnimation(portraitData.animationAddress);
    auto animationAddress = portraitData.animationAddress;
    characterPortraitResource.graphicsOffsetHalved =
            doesAnimationHaveHalvedGraphicsOffsets(animationAddress);
    return characterPortraitResource;
}

void AdMemoryHandler::loadPortraitResourceIntoVRam(
        MemoryLoadInfo const& memoryLoadInfo)
{
    auto portraitTextureResource = adCdImageReader_->readSectors(
                memoryLoadInfo.sector,
                memoryLoadInfo.sectorsNumber);
    AdResourcesIterator resourcesIterator(portraitTextureResource);
    int portraitImageIndex = 0;
    while (resourcesIterator.hasNext())
    {
        if (portraitImageIndex > 2)
        {
            throw QString(
                        "Max number of resources (2) per portrait resources "
                        "excedeed.");
        }
        Rect portraitTextureRect;
        ram_->readRegion(
                    {
                        0x8006b220 +
                        portraitImageIndex * sizeof(portraitTextureRect),
                        sizeof (portraitTextureRect)
                    },
                    reinterpret_cast<uint8_t*>(&portraitTextureRect));
        auto nextResourceDescriptor = resourcesIterator.next();
        auto const* resourceHeader = nextResourceDescriptor.header;
        auto const* resourceData = nextResourceDescriptor.data;
        auto resourceSize = nextResourceDescriptor.size;
        if (resourceHeader->type == ResourceType::PackedImage)
        {
            auto resourceTexture =
                    unpackResourceTexture(resourceData, resourceSize);
            vram().load(
                        resourceTexture,
                        portraitTextureRect.toQRect());
        }
        else
        {
            throw QString("Unexpected portrait resource type %1.")
                    .arg(static_cast<uint16_t>(resourceHeader->type));
        }
        ++portraitImageIndex;
    }
}

AnimationFrames AdMemoryHandler::readAnimation(PsxRamAddress animationAddress)
{
    AnimationFrames animationFrames;
    AnimationFrame animationFrame;
    auto& animation = animationFrame.first;
    while (true)
    {
        ram().readRegion(
                    { animationAddress, sizeof(animation) },
                    reinterpret_cast<uint8_t*>(&animation));
        if (animation.frameType != AnimationFrameType::NotLastFrame)
        { break; }
        animationFrame.second = readGraphicsSeries(animation.graphicAddress);
        animationFrames.append(animationFrame);
        animationAddress += sizeof(Animation);
    }
    return animationFrames;
}

GraphicsSeries AdMemoryHandler::readGraphicsSeries(PsxRamAddress graphicAddress)
{
    GraphicsSeries graphicsSeries;
    Graphic graphic;
    do
    {
        ram_->readRegion(
                    {graphicAddress, sizeof(graphic)},
                    reinterpret_cast<uint8_t*>(&graphic));
        auto graphicImage = readGraphic(graphic);
        graphicsSeries.append({graphic, graphicImage});
        graphicAddress += sizeof(Graphic);
    }
    while (!graphic.hasFlags(GraphicFlags::SeriesEnd));
    return graphicsSeries;
}

QImage AdMemoryHandler::readGraphic(Graphic const& graphic)
{
    auto image = readPlainGraphic(graphic);
    bool flipHorizontally = graphic.hasFlags(GraphicFlags::FlipHorizontally);
    bool flipVertically = graphic.hasFlags(GraphicFlags::FlipVertically);
    if (flipHorizontally || flipVertically)
    { return image.mirrored(flipHorizontally, flipVertically); }
    else
    { return image;}
}

QImage AdMemoryHandler::readPlainGraphic(Graphic const& graphic)
{
    switch (graphic.texpage.texpageBpp())
    {
    case TexpageBpp::BPP_4:
    {
        VirtualPsxVRam::Palette4Bpp palette;
        vram_->read4BppPalette(graphic.clut, palette);
        return vram_->read4BppTexture(graphic, &palette);
    }
    case TexpageBpp::BPP_8:
    {
        VirtualPsxVRam::Palette8Bpp palette;
        vram_->read8BppPalette(graphic.clut, palette);
        return vram_->read8BppTexture(graphic, &palette);
    }
    case TexpageBpp::BPP_15:
        return vram_->read16BppTexture(graphic);
    default:
        throw QString("Unknown graphic Bpp (%1).").arg(graphic.texpage.bpp);
    }
}

bool AdMemoryHandler::doesAnimationHaveHalvedGraphicsOffsets(
        PsxRamAddress animationAddress)
{
    return animationAddress == PsxRamAddress(0x80077784) ||
            animationAddress == PsxRamAddress(0x800776fc);
}

QImage AdMemoryHandler::combinePortraitGraphicSeries(
        GraphicsSeries const& graphicsSeries,
        PortraitData const& portraitData)
{
    auto animationAddress = portraitData.animationAddress;
    return combineGraphicSeries(
                graphicsSeries,
                doesAnimationHaveHalvedGraphicsOffsets(animationAddress));
}

QImage AdMemoryHandler::combineGraphicSeries(
        GraphicsSeries const& graphicSeries,
        bool graphicOffsetHalved)
{
    auto graphicSeriesBounds =
            calculateGraphicsSeriesBounds(graphicSeries, graphicOffsetHalved);
    auto anchorPoint = -graphicSeriesBounds.topLeft();
    return combineGraphicSeries(
                graphicSeries,
                graphicOffsetHalved,
                anchorPoint,
                graphicSeriesBounds.size());
}

QRect AdMemoryHandler::calculateGraphicsSeriesBounds(
        GraphicsSeries const& graphicsSeries,
        bool graphicOffsetHalved)
{
    if (graphicsSeries.isEmpty())
    { return {}; }
    auto calculateGraphicBounds = [=](Graphic const& graphic) {
        auto position = calculateGraphicPosition(graphic, graphicOffsetHalved);
        QSize size(graphic.width, graphic.height);
        return QRect(position, size);
    };
    auto it = graphicsSeries.begin();
    auto bounds = calculateGraphicBounds((*it).first);
    ++it;
    for (; it != graphicsSeries.end(); ++it)
    { bounds = bounds.united(calculateGraphicBounds((*it).first)); }
    return bounds;
}

QImage AdMemoryHandler::combineGraphicSeries(
        GraphicsSeries const& graphicsSeries,
        bool graphicOffsetHalved,
        QPoint const& anchorPoint,
        QSize const& size)
{
    if (graphicsSeries.isEmpty())
    { return QImage(); }
    QImage combinedImage(size, QImage::Format_ARGB32);
    combinedImage.fill(Qt::transparent);
    QPainter combinedImagePainter(&combinedImage);
    for (auto it = graphicsSeries.rbegin(); it != graphicsSeries.rend(); ++it)
    {
        auto const& graphic = (*it).first;
        auto const& image = (*it).second;
        auto imagePosition =
                calculateGraphicInSeriesPosition(
                    graphic,
                    graphicOffsetHalved,
                    anchorPoint);
        combinedImagePainter.drawImage(imagePosition, image);
    }
    return combinedImage;
}

QPoint AdMemoryHandler::calculateGraphicPosition(
        Graphic const& graphic,
        bool graphicOffsetHalved)
{
    QPoint graphicPosition(graphic.xOffset, graphic.yOffset);
    if (graphic.hasFlags(GraphicFlags::FlipHorizontally))
    { graphicPosition.rx() = -graphicPosition.rx() - graphic.width; }
    if (graphic.hasFlags(GraphicFlags::FlipVertically))
    { graphicPosition.ry() = -graphicPosition.ry() - graphic.height; }
    if (graphicOffsetHalved)
    { graphicPosition *= 2; }
    return graphicPosition;
}

QPoint AdMemoryHandler::calculateGraphicInSeriesPosition(
        Graphic const& graphic,
        bool graphicOffsetHalved,
        QPoint const& anchorPoint)
{ return calculateGraphicPosition(graphic, graphicOffsetHalved) + anchorPoint; }
