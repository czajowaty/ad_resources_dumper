#ifndef ADMEMORYHANDLER_HPP
#define ADMEMORYHANDLER_HPP

#include "AdDefinitions.hpp"
#include "AdSpeakerId.hpp"
#include "BinCdImageReader.hpp"
#include "VirtualPsxRam.hpp"
#include "VirtualPsxVRam.hpp"
#include <QImage>
#include <QVector>
#include <memory>

struct CharacterPortraitData
{
    PsxRamAddress portraitDataAddress;
    PortraitData portraitData;
};

using CharacterPortraitsData = QVector<CharacterPortraitData>;
using GraphicsSeries = QVector<QPair<Graphic, QImage>>;
using AnimationFrame = QPair<Animation, GraphicsSeries>;
using AnimationFrames = QVector<AnimationFrame>;

struct CharacterPortraitResource
{
    MemoryLoadInfo resourcesMemoryLoadInfo;
    AnimationFrames animationFrames;
    bool graphicsOffsetHalved;
};

struct SpeakerInfo
{
    char const* name;
    AdSpeakerId speakerId;
    uint32_t variantsNumber;

    bool isValid() const
    { return variantsNumber > 0; }
};

using SpeakersInfo = std::array<SpeakerInfo, 15>;

static constexpr SpeakersInfo SPEAKERS_INFO = {
    {
        {"Aunt", AdSpeakerId::Aunt, 2},
        {"Cherrl", AdSpeakerId::Cherrl, 9},
        {"Fur", AdSpeakerId::Fur, 6},
        {"Ghosh", AdSpeakerId::Ghosh, 3},
        {"Grandpa", AdSpeakerId::Grandpa, 1},
        {"Guy", AdSpeakerId::Guy, 3},
        {"Jorda", AdSpeakerId::Jorda, 1},
        {"Kewne", AdSpeakerId::Kewne, 13},
        {"Miya", AdSpeakerId::Miya, 7},
        {"Nico", AdSpeakerId::Nico, 8},
        {"Patty", AdSpeakerId::Patty, 5},
        {"Selfi", AdSpeakerId::Selfi, 6},
        {"Vivian", AdSpeakerId::Vivian, 6},
        {"Weedy", AdSpeakerId::Weedy, 2},
        {"Wreath", AdSpeakerId::Wreath, 4}
    }
};

class AdMemoryHandler
{
    static constexpr uint8_t INVALID_SPEAKER_PORTRAIT_INDEX = 0xff;
    static const QPoint PORTRAIT_POSITION;
    static constexpr SpeakerInfo INVALID_SPEAKER_INFO =
        {nullptr, AdSpeakerId::None, 0};

public:
    AdMemoryHandler();

    VirtualPsxRam const& ram() const
    { return *ram_; }
    VirtualPsxVRam const& vram() const
    { return *vram_; }
    // TODO: remove them?
    VirtualPsxVRam& vram()
    { return *vram_; }
    // TODO: remove them?
    BinCdImageReader const* adCdImageReader() const
    { return adCdImageReader_.get(); }
    // TODO: remove them?
    BinCdImageReader* adCdImageReader()
    { return adCdImageReader_.get(); }
    void loadCdImage(QString const& cdImagePath);
    void loadGameModeResources(GameMode gameMode);
    CharacterPortraitsData readCharacterPortraitsData(AdSpeakerId speakerId);
    CharacterPortraitResource loadCharacterPortrait(PortraitData portraitData);
    AnimationFrames readAnimation(PsxRamAddress animationAddress);
    GraphicsSeries readGraphicsSeries(PsxRamAddress graphicAddress);
    QImage readGraphic(Graphic const& graphic);
    QImage readPlainGraphic(Graphic const& graphic);
    QImage combinePortraitGraphicSeries(
            GraphicsSeries const& graphicsSeries,
            PortraitData const& portraitData);
    QImage combineGraphicSeries(
            GraphicsSeries const& graphicsSeries,
            bool graphicOffsetHalved);
    QImage combineGraphicSeries(
            GraphicsSeries const& graphicsSeries,
            bool graphicOffsetHalved,
            QPoint const& anchorPoint,
            QSize const& size);

private:
    void loadSlusTextSection();
    void loadTownResources();
    GameModeData readGameModeData(GameMode gameMode) const;
    uint32_t readGameModeDataSectorsNumber(GameMode gameMode);
    uint32_t gameModeToIndex(GameMode gameMode) const
    { return static_cast<uint32_t>(gameMode); }
    void loadTownVRamResources();
    void loadVRamLoadablePackedImage(
            ResourceType1Header const* resourceHeader,
            uint8_t const* resourceData,
            uint32_t maxSize);
    QByteArray unpackResourceTexture(
            uint8_t const* resourceData,
            uint32_t maxSize);
    void loadVRamPalette(
            ResourceType2Header const* resourceHeader,
            uint8_t const* resourceData);
    uint8_t readSpeakerPortraitIndex(AdSpeakerId speakerId);
    SpeakerInfo const& findSpeakerInfo(AdSpeakerId speakerId) const;
    void loadPortraitResourceIntoVRam(MemoryLoadInfo const& memoryLoadInfo);
    bool doesAnimationHaveHalvedGraphicsOffsets(PsxRamAddress animationAddress);
    QRect calculateGraphicsSeriesBounds(
            GraphicsSeries const& graphicsSeries,
            bool graphicOffsetHalved);
    QPoint calculateGraphicPosition(
            Graphic const& graphic,
            bool graphicOffsetHalved);
    QPoint calculateGraphicInSeriesPosition(
            Graphic const& graphic,
            bool graphicOffsetHalved,
            QPoint const& anchorPoint);

    std::unique_ptr<VirtualPsxRam> ram_;
    std::unique_ptr<VirtualPsxVRam> vram_;
    std::unique_ptr<BinCdImageReader> adCdImageReader_;
};

#endif // ADMEMORYHANDLER_HPP
