#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include "AdDefinitions.hpp"
#include "AdSpeakerId.hpp"
#include "BinCdImageReader.hpp"
#include "VirtualPsxRam.hpp"
#include "VirtualPsxVRam.hpp"
#include <QMainWindow>
#include <QSettings>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

    static constexpr uint8_t INVALID_SPEAKER_PORTRAIT_INDEX = 0xff;

    struct SpeakerInfo
    {
        char const* name;
        AdSpeakerId speakerId;
        uint32_t variantsNumber;
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
            {"Selfi", AdSpeakerId::Selfi, 7},
            {"Vivian", AdSpeakerId::Vivian, 6},
            {"Weedy", AdSpeakerId::Weedy, 2},
            {"Wreath", AdSpeakerId::Wreath, 4}
        }
    };

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent* event);

private slots:
    void on_action_OpenCdImage_triggered();
    void on_actionSaveAlImages_triggered();
    void onCharacterSelectionChanged();
    void onVariantSelectionChanged();

private:
    void loadSlusTextSection();
    void loadTownResources();
    void loadGameModeResources(GameMode gameMode);
    void loadTownVRamResources();
    void showVRam();
    void loadVRamLoadablePackedImage(
            ResourceType1Header const* resourceHeader,
            uint8_t const* resourceData);
    QByteArray unpackResourceTexture(uint8_t const* resourceData);
    void loadVRamPalette(
            ResourceType2Header const* resourceHeader,
            uint8_t const* resourceData);
    void onVariantSelectionCleared();
    void clearPortrait();
    QImage loadPortrait(
            SpeakerInfo speakerInfo,
            int portraitVariant,
            bool updatePortraitInfo=false);
    void handleLoadPortraitError(QString const& error);
    uint8_t getSpeakerPortraitIndex(AdSpeakerId speakerId);
    void loadPortraitImageResource(MemoryLoadInfo const& memoryLoadInfo);
    QImage loadGraphic(Graphic const& graphic);
    QString toQString(PsxRamAddress const& psxRamAddress);
    QString toQString(TexpageBpp bpp);
    uint8_t* unpack(uint8_t const* src, uint8_t* dest);

    Ui::MainWindow* ui_;
    QSettings globalSettings_;
    QString lastOpenCdImagePath_;
    int selectedSpeakerIndex_{-1};
    std::unique_ptr<BinCdImageReader> binCdImageReader_;
    std::unique_ptr<VirtualPsxRam> virtualPsxRam_;
    std::unique_ptr<VirtualPsxVRam> virtualPsxVRam_;
};
#endif // MAINWINDOW_HPP
