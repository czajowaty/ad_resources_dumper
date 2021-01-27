#include "MainWindow.hpp"
#include "ui_MainWindow.h"
#include "AdResourcesIterator.hpp"
#include <QCloseEvent>
#include <QFileDialog>
#include <QMessageBox>

// TODO: remove it
#include <QDebug>

constexpr MainWindow::SpeakersInfo MainWindow::SPEAKERS_INFO;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      ui_{new Ui::MainWindow},
      virtualPsxRam_(new VirtualPsxRam()),
      virtualPsxVRam_(new VirtualPsxVRam())
{
    ui_->setupUi(this);
    globalSettings_.beginGroup("main_window");
    resize(globalSettings_.value("size", QSize(1024, 768)).toSize());
    move(globalSettings_.value("pos", QPoint(200, 100)).toPoint());
    globalSettings_.endGroup();
    lastOpenCdImagePath_ =
            globalSettings_.value("last_open_cd_image_path").toString();
    ui_->textureLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    ui_->textureLabel->setScaledContents(false);
    ui_->vramImageLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    ui_->vramImageLabel->setScaledContents(false);
    for (auto const& speakerInfo : SPEAKERS_INFO)
    { ui_->characterListWidget->addItem(speakerInfo.name); }
    connect(
                ui_->characterListWidget, &QListWidget::itemSelectionChanged,
                this, &MainWindow::onCharacterSelectionChanged);
    connect(
                ui_->variantsListWidget, &QListWidget::itemSelectionChanged,
                this, &MainWindow::onVariantSelectionChanged);
    connect(
                ui_->spinBox, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int) { onVariantSelectionChanged(); });
}

MainWindow::~MainWindow()
{ delete ui_; }

void MainWindow::closeEvent(QCloseEvent* event)
{
    globalSettings_.beginGroup("main_window");
    globalSettings_.setValue("size", size());
    globalSettings_.setValue("pos", pos());
    globalSettings_.endGroup();
    globalSettings_.setValue("last_open_cd_image_path", lastOpenCdImagePath_);
    event->accept();
}

void MainWindow::on_action_OpenCdImage_triggered()
{
    auto filePath = QFileDialog::getOpenFileName(
                this,
                "Select CD image",
                lastOpenCdImagePath_,
                "BIN files (*.bin)");
    if (filePath.isEmpty())
    { return; }
    try
    {
        binCdImageReader_ = BinCdImageReader::create(filePath);
        lastOpenCdImagePath_ = filePath;
        virtualPsxRam_->clear();
        loadSlusTextSection();
        loadTownResources();
        showVRam();
    }
    catch (QString const& error)
    { QMessageBox::critical(this, "CD image open error", error); }
}

void MainWindow::loadSlusTextSection()
{
    static constexpr uint32_t SLUS_TEXT_SECTION_START_SECTOR = 25;
    static constexpr uint32_t SLUS_TEXT_SECTION_SIZE = 0x54800;
    static constexpr PsxRamAddress::Raw SLUS_TEXT_SECTION_LOAD_ADDRESS =
            0x8002d000;
    virtualPsxRam_->load(
                binCdImageReader_->readSectors(
                    SLUS_TEXT_SECTION_START_SECTOR,
                    BinCdImageReader::calculateSectorsNumber(
                        SLUS_TEXT_SECTION_SIZE)),
                SLUS_TEXT_SECTION_LOAD_ADDRESS);
}

void MainWindow::loadTownResources()
{
    loadGameModeResources(GameMode::InTown);
    loadTownVRamResources();
}

void MainWindow::loadGameModeResources(GameMode gameMode)
{
    uint32_t gameModeIndex = static_cast<uint32_t>(gameMode);
    uint32_t sectorsNumber = virtualPsxRam_->readDWord(
                0x8006ce6c + (gameModeIndex << 2));
    GameModeData gameModeData;
    virtualPsxRam_->readRegion(
                {
                    0x8006ce44 + (gameModeIndex * sizeof(GameModeData)),
                    sizeof(GameModeData)
                },
                reinterpret_cast<uint8_t*>(&gameModeData));
    if (gameModeData.memoryLoadInfoAddress.isNull())
    { return; }
    MemoryLoadInfo memoryLoadInfo;
    virtualPsxRam_->readRegion(
                { gameModeData.memoryLoadInfoAddress, sizeof(MemoryLoadInfo)},
                reinterpret_cast<uint8_t*>(&memoryLoadInfo));
    PsxRamAddress memoryLoadInfoAddress = memoryLoadInfo.address;
    if (memoryLoadInfoAddress.isNull())
    { throw QString("Address to where load resources is null."); }
    memoryLoadInfoAddress |= PsxRamConst::KSEG0_ADDRESS;
    sectorsNumber &= ~0x1ff;
    sectorsNumber |= memoryLoadInfo.sectorsNumber;
    virtualPsxRam_->load(
                binCdImageReader_->readSectors(
                    memoryLoadInfo.sector,
                    sectorsNumber),
                memoryLoadInfoAddress);
    for (uint32_t i = 0; i < 5; ++i)
    {
        QString s;
        for (uint32_t j = 0; j < 0x10; ++j)
        {
            s += QString("%1 ")
                    .arg(
                        virtualPsxRam_->readByte(
                            memoryLoadInfoAddress + i * 0x10 + j),
                        2,
                        16,
                        QChar('0'));
        }
        qDebug() << QString("%1: %2").arg(i, 2, 16, QChar('0')).arg(s);
    }
}

void MainWindow::loadTownVRamResources()
{
    MemoryLoadInfo townResourcesMemoryLoadInfo;
    virtualPsxRam_->readRegion(

                {0x80080ea0, sizeof(townResourcesMemoryLoadInfo)},
                reinterpret_cast<uint8_t*>(&townResourcesMemoryLoadInfo));
    auto townResourcesData = binCdImageReader_->readSectors(
                townResourcesMemoryLoadInfo.sector,
                townResourcesMemoryLoadInfo.sectorsNumber);
    AdResourcesIterator resourcesIterator(townResourcesData);
    while (resourcesIterator.hasNext())
    {
        auto nextResource = resourcesIterator.next();
        auto const* resourceHeader = nextResource.first;
        auto const* resourceData = nextResource.second;
        if (resourceHeader->type == ResourceType::VRamLoadablePackedImage)
        {
            loadVRamLoadablePackedImage(
                        reinterpret_cast<ResourceType1Header const*>(
                            resourceHeader),
                        resourceData);
        }
        else if (resourceHeader->type == ResourceType::Palette)
        {
            loadVRamPalette(
                        reinterpret_cast<ResourceType2Header const*>(
                            resourceHeader),
                        resourceData);
        }
        else
        {
            throw QString("Unexpected town resource type %1.")
                    .arg(static_cast<uint16_t>(resourceHeader->type));
        }
    }
}

void MainWindow::showVRam()
{
    auto vramImage = virtualPsxVRam_->asImage();
    ui_->vramImageLabel->setPixmap(QPixmap::fromImage(vramImage));
    ui_->vramImageLabel->adjustSize();
}

void MainWindow::loadVRamLoadablePackedImage(
        ResourceType1Header const* resourceHeader,
        uint8_t const* resourceData)
{
    auto resourceTexture = unpackResourceTexture(resourceData);
    virtualPsxVRam_->load(resourceTexture, resourceHeader->rect.toQRect());
}

QByteArray MainWindow::unpackResourceTexture(uint8_t const* resourceData)
{
    QByteArray unpackedResourceTexture;
    unpackedResourceTexture.resize(0x2000);
    uint8_t* unpackedResourceData =
            reinterpret_cast<uint8_t*>(unpackedResourceTexture.data());
    auto* unpackedResourceDataEnd = unpack(resourceData, unpackedResourceData);
    auto unpackedDataSize = unpackedResourceDataEnd - unpackedResourceData;
    if (unpackedDataSize > unpackedResourceTexture.size())
    {
        throw QString(
                    "Too large (0x%1 bytes) resource texture. "
                    "Max size 0x%2 bytes.")
                .arg(unpackedDataSize, 0, 16)
                .arg(unpackedResourceTexture.size());
    }
    unpackedResourceTexture.resize(unpackedDataSize);
    return unpackedResourceTexture;
}

void MainWindow::loadVRamPalette(
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
    virtualPsxVRam_->load(
                resourceData,
                resourceHeader->numberOf4BppPalettes * palette4BppRawDataSize,
                paletteRect);
}


void MainWindow::on_actionSaveAlImages_triggered()
{
    for (auto const& speakerInfo : SPEAKERS_INFO)
    {
        for (
             uint32_t portraitVariant = 0;
             portraitVariant < speakerInfo.variantsNumber;
             ++portraitVariant)
        {
            try
            {
                auto portraitImage = loadPortrait(speakerInfo, portraitVariant);
                if (!portraitImage.isNull())
                {
                    portraitImage.save(
                                QString("%1_%2.png")
                                .arg(speakerInfo.name)
                                .arg(portraitVariant),
                                "PNG");
                }
            }
            catch (QString const&)
            {
                // TODO: maybe add error message at the end of saving
            }
        }
    }
}

void MainWindow::onCharacterSelectionChanged()
{
    clearPortrait();
    ui_->variantsListWidget->clear();
    auto selectedItems = ui_->characterListWidget->selectedItems();
    if (selectedItems.empty())
    { return; }
    auto selectedItem = selectedItems.first();
    selectedSpeakerIndex_ = ui_->characterListWidget->row(selectedItem);
    if (static_cast<uint32_t>(selectedSpeakerIndex_) >= SPEAKERS_INFO.size())
    {
        QMessageBox::critical(
                    this,
                    "Error",
                    QString("Selected character has unexpected index %1.")
                    .arg(selectedSpeakerIndex_));
        return;
    }
    auto const& speakerInfo = SPEAKERS_INFO[selectedSpeakerIndex_];
    for (
         uint32_t variantIndex = 0;
         variantIndex < speakerInfo.variantsNumber;
         ++variantIndex)
    { ui_->variantsListWidget->addItem(QString::number(variantIndex)); }
}

void MainWindow::onVariantSelectionChanged()
{
    auto selectedItems = ui_->variantsListWidget->selectedItems();
    if (selectedItems.empty())
    {
        onVariantSelectionCleared();
        return;
    }
    auto portraitVariant = ui_->variantsListWidget->row(selectedItems.first());
    try
    {
        auto portraitImage =
                loadPortrait(
                    SPEAKERS_INFO[selectedSpeakerIndex_],
                    portraitVariant,
                    true);
        ui_->textureLabel->setPixmap(QPixmap::fromImage(portraitImage));
        ui_->textureLabel->adjustSize();
    }
    catch (QString const& error)
    {
        QMessageBox::warning(this, "Load portrait", error);
        clearPortrait();
    }
}

void MainWindow::onVariantSelectionCleared()
{ clearPortrait(); }

void MainWindow::clearPortrait()
{
    ui_->textureLabel->setPixmap(QPixmap());
    ui_->portraitInfoTextEdit->setText("");
}

QImage MainWindow::loadPortrait(
        SpeakerInfo speakerInfo,
        int portraitVariant,
        bool updatePortraitInfo)
{
    auto insertPortraitInfoLine = [=](QString const& text) {
        if (!updatePortraitInfo)
        { return; }
        ui_->portraitInfoTextEdit->insertPlainText(text);
        ui_->portraitInfoTextEdit->insertPlainText("\n");
    };

    if (updatePortraitInfo)
    { ui_->portraitInfoTextEdit->setText(""); }
    auto speakerPortraitIndex = getSpeakerPortraitIndex(speakerInfo.speakerId);
    if (speakerPortraitIndex == INVALID_SPEAKER_PORTRAIT_INDEX)
    { throw QString("'%1' does not have portrait.").arg(speakerInfo.name); }
    QString portraitInfoText;
    PsxRamAddress portraitDataTableAddressAddress =
            0x8006b1a8 + speakerPortraitIndex * sizeof(PsxRamAddress);
    insertPortraitInfoLine(
                "Address of portrait data table address: " +
                toQString(portraitDataTableAddressAddress));
    auto portraitDataTableAddress = virtualPsxRam_->readAddress(
                portraitDataTableAddressAddress);
    if (portraitDataTableAddress.isNull())
    { throw QString("'%1' does not have portrait.").arg(speakerInfo.name); }
    PsxRamAddress portraitDataAddress =
            portraitDataTableAddress + (portraitVariant * sizeof(PortraitData));
    insertPortraitInfoLine(
                "Portrait data address: " + toQString(portraitDataAddress));
    PortraitData portraitData;
    virtualPsxRam_->readRegion(
                {portraitDataAddress, sizeof(portraitData)},
                reinterpret_cast<uint8_t*>(&portraitData));
    insertPortraitInfoLine(
                "Memory load info address: " +
                toQString(portraitData.portraitMemoryLoadInfoAddress));
    insertPortraitInfoLine(
                "Animation address: " +
                toQString(portraitData.animationAddress));
    if (portraitData.portraitMemoryLoadInfoAddress.isNull())
    {
        throw QString("'%1' does not have portrait for variant %2.")
                .arg(speakerInfo.name)
                .arg(portraitVariant);
    }
    MemoryLoadInfo memoryLoadInfo;
    virtualPsxRam_->readRegion(
                {
                    portraitData.portraitMemoryLoadInfoAddress,
                    sizeof(memoryLoadInfo)
                },
                reinterpret_cast<uint8_t*>(&memoryLoadInfo));
    insertPortraitInfoLine("Memory load info:");
    insertPortraitInfoLine(
                QString("  Sector: 0x%1").arg(memoryLoadInfo.sector, 0, 16));
    insertPortraitInfoLine(
                QString("  Sectors number: 0x%1")
                .arg(memoryLoadInfo.sectorsNumber, 0, 16));
    Animation animation;
    virtualPsxRam_->readRegion(
                { portraitData.animationAddress, sizeof(animation) },
                reinterpret_cast<uint8_t*>(&animation));
    insertPortraitInfoLine("Animation:");
    insertPortraitInfoLine(QString("  Duration: %1").arg(animation.duration));
    insertPortraitInfoLine(
                "  Graphic address: " + toQString(animation.graphicAddress));
    uint32_t animationStep = ui_->spinBox->value();
    PsxRamAddress graphicAddress = virtualPsxRam_->readAddress(
                portraitData.animationAddress + 4u + animationStep * 8);
    Graphic graphic;
    virtualPsxRam_->readRegion(
                //{ animation.graphicAddress, sizeof(graphic) },
                { graphicAddress, sizeof(graphic) },
                reinterpret_cast<uint8_t*>(&graphic));
    auto const& texpage = graphic.texpage;
    insertPortraitInfoLine("Graphic:");
    insertPortraitInfoLine(
                QString("  Effects: 0x%1").arg(graphic.effects, 0, 16));
    insertPortraitInfoLine(
                QString("  BPP: %1").arg(toQString(texpage.texpageBpp())));
    insertPortraitInfoLine(
                QString("  Texture page: 0x%1, 0x%2")
                .arg(texpage.x, 0, 16)
                .arg(texpage.y, 0, 16));
    insertPortraitInfoLine(
                QString("  CLUT coordinates: 0x%1, 0x%2")
                .arg(graphic.clut.x)
                .arg(graphic.clut.y));
    insertPortraitInfoLine(
                QString("  Position: 0x%1, 0x%2")
                .arg(graphic.xOffset, 0, 16)
                .arg(graphic.yOffset, 0, 16));
    insertPortraitInfoLine(
                QString(
                    "  In-texture page bounds: x=0x%1, y=0x%2, w=0x%3, h=0x%4")
                .arg(graphic.xOffsetInTexpage, 0, 16)
                .arg(graphic.yOffsetInTexpage, 0, 16)
                .arg(graphic.width, 0, 16)
                .arg(graphic.height, 0, 16));
    auto vramPortraitRect = virtualPsxVRam_->calculateVRamRect(graphic);
    insertPortraitInfoLine(
                QString("  VRam bounds: x=0x%1, y=0x%2, w=0x%3, h=0x%4")
                .arg(vramPortraitRect.x(), 0, 16)
                .arg(vramPortraitRect.y(), 0, 16)
                .arg(vramPortraitRect.width(), 0, 16)
                .arg(vramPortraitRect.height(), 0, 16));
    try
    { loadPortraitImageResource(memoryLoadInfo); }
    catch (QString const& error)
    {
        throw QString("Error while loading portrait image resource. %1.")
                .arg(error);
    }
    try
    { return loadGraphic(graphic); }
    catch (QString const& error)
    { throw QString("Error while reading portrait image.").arg(error); }
}

uint8_t MainWindow::getSpeakerPortraitIndex(AdSpeakerId speakerId)
{
    PsxRamAddress speakersWithPortraitsAddress = 0x8006b1ec;
    for (uint8_t index = 0; index < 18; ++index)
    {
        auto portraitSpeakerId =
                virtualPsxRam_->readByte(
                    speakersWithPortraitsAddress.raw() + index);
        if (speakerId == static_cast<AdSpeakerId>(portraitSpeakerId))
        { return index; }
    }
    return INVALID_SPEAKER_PORTRAIT_INDEX;
}

void MainWindow::loadPortraitImageResource(
        MemoryLoadInfo const& memoryLoadInfo)
{
    auto portraitTextureResource = binCdImageReader_->readSectors(
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
        virtualPsxRam_->readRegion(
                    {
                        0x8006b220 +
                        portraitImageIndex * sizeof(portraitTextureRect),
                        sizeof (portraitTextureRect)
                    },
                    reinterpret_cast<uint8_t*>(&portraitTextureRect));
        auto nextResource = resourcesIterator.next();
        auto const* resourceHeader = nextResource.first;
        auto const* resourceData = nextResource.second;
        if (resourceHeader->type == ResourceType::PackedImage)
        {
            auto resourceTexture = unpackResourceTexture(resourceData);
            virtualPsxVRam_->load(
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
    showVRam();
}

QImage MainWindow::loadGraphic(Graphic const& graphic)
{
    switch (graphic.texpage.texpageBpp())
    {
    case TexpageBpp::BPP_4:
    {
        VirtualPsxVRam::Palette4Bpp palette;
        virtualPsxVRam_->read4BppPalette(graphic.clut, palette);
        return virtualPsxVRam_->read4BppTexture(graphic, &palette);
    }
    case TexpageBpp::BPP_8:
    {
        VirtualPsxVRam::Palette8Bpp palette;
        virtualPsxVRam_->read8BppPalette(graphic.clut, palette);
        return virtualPsxVRam_->read8BppTexture(graphic, &palette);
    }
    case TexpageBpp::BPP_15:
        return virtualPsxVRam_->read16BppTexture(graphic);
    default:
        throw QString("Unknown graphic Bpp (%1).").arg(graphic.texpage.bpp);
    }
}

QString MainWindow::toQString(PsxRamAddress const& psxRamAddress)
{ return QString("0x%1").arg(psxRamAddress.raw(), 8, 16, QChar('0')); }

QString MainWindow::toQString(TexpageBpp bpp)
{
    switch (bpp)
    {
    case TexpageBpp::BPP_4:
        return "4 bpp";
    case TexpageBpp::BPP_8:
        return "8 bpp";
    case TexpageBpp::BPP_15:
        return "15 bpp";
    default:
        return QString("Unknown (%1)").arg(static_cast<uint16_t>(bpp));
    }
}

uint8_t* MainWindow::unpack(uint8_t const* src, uint8_t* dest)
{
  uint8_t bVar1;
  uint16_t uVar2;
  int straightBytesToReadCount;
  uint8_t const* nextSrc;
  uint32_t uVar3;
  uint32_t valueRead;
  int index;

  valueRead = 0;
  index = 1;
  do {
    while( true ) {
      index = index + -1;
      nextSrc = src;
      if (index == 0) {
        valueRead = (uint32_t)*src;
        nextSrc = src + 1;
        index = 8;
      }
      if ((valueRead & 1) != 0) break;
      valueRead = valueRead >> 1;
      src = nextSrc + 1;
      *dest = *nextSrc;
      dest = dest + 1;
    }
    index = index + -1;
    valueRead = valueRead >> 1;
    if (index == 0) {
      valueRead = (uint32_t)*nextSrc;
      nextSrc = nextSrc + 1;
      index = 8;
    }
    if ((valueRead & 1) == 0) {
      uVar2 = (nextSrc[0] << 8) | nextSrc[1];
      valueRead = valueRead >> 1;
      if (uVar2 == 0) {
        return dest;
      }
      src = nextSrc + 2;
      if ((nextSrc[1] & 0xf) == 0) {
        bVar1 = *src;
        src = nextSrc + 3;
        straightBytesToReadCount = (uint32_t)bVar1 + 1;
      }
      else {
        straightBytesToReadCount = ((uint32_t)uVar2 & 0xf) + 2;
      }
      uVar3 = (uint32_t)(uVar2 >> 4);
    }
    else {
      index = index + -1;
      valueRead = valueRead >> 1;
      if (index == 0) {
        valueRead = (uint32_t)*nextSrc;
        nextSrc = nextSrc + 1;
        index = 8;
      }
      index = index + -1;
      uVar3 = valueRead >> 1;
      if (index == 0) {
        uVar3 = (uint32_t)*nextSrc;
        nextSrc = nextSrc + 1;
        index = 8;
      }
      straightBytesToReadCount = (valueRead & 1) * 2 + 2 + (uVar3 & 1);
      valueRead = uVar3 >> 1;
      uVar3 = (uint32_t)*nextSrc;
      src = nextSrc + 1;
      if (*nextSrc == 0) {
        uVar3 = 0x100;
      }
    }
    nextSrc = dest + -uVar3;
    while (straightBytesToReadCount != 0) {
      bVar1 = *nextSrc;
      nextSrc = nextSrc + 1;
      straightBytesToReadCount = straightBytesToReadCount + -1;
      *dest = bVar1;
      dest = dest + 1;
    }
  } while( true );
}
