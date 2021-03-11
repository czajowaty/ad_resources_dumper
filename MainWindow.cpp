#include "MainWindow.hpp"
#include "ui_MainWindow.h"
#include "AdResourceUnpacker.hpp"
#include "AdResourcesIterator.hpp"
#include <QCloseEvent>
#include <QFileDialog>
#include <QMessageBox>
#include <QProgressDialog>
#include <QtEndian>

// TODO: remove it
#include <QDebug>

// TODO: subscribe to vram changes

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      ui_{new Ui::MainWindow},
      adMemoryHandler_{std::make_unique<AdMemoryHandler>()}
{
    ui_->setupUi(this);
    ui_->portraitScrollArea->setWidgetResizable(false);
    globalSettings_.beginGroup("main_window");
    resize(globalSettings_.value("size", QSize(1024, 768)).toSize());
    move(globalSettings_.value("pos", QPoint(200, 100)).toPoint());
    if (globalSettings_.value("is_maximized", false).toBool())
    { showMaximized(); }
    globalSettings_.endGroup();
    lastOpenCdImagePath_ =
            globalSettings_.value("last_open_cd_image_path").toString();
    ui_->vramImageLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    ui_->vramImageLabel->setScaledContents(false);
    for (auto const& speakerInfo : SPEAKERS_INFO)
    {
        ui_->characterListWidget->addItem(
                    QString("%1 (%2)")
                    .arg(speakerInfo.name)
                    .arg(static_cast<uint16_t>(speakerInfo.speakerId)));
    }
    connect(
                ui_->characterListWidget, &QListWidget::itemSelectionChanged,
                this, &MainWindow::onCharacterSelectionChanged);
    connect(
                ui_->variantsListWidget, &QListWidget::itemSelectionChanged,
                this, &MainWindow::onVariantSelectionChanged);
    on_action_OpenCdImage_triggered();
}

MainWindow::~MainWindow()
{ delete ui_; }

void MainWindow::closeEvent(QCloseEvent* event)
{
    globalSettings_.beginGroup("main_window");
    globalSettings_.setValue("size", size());
    globalSettings_.setValue("pos", pos());
    globalSettings_.setValue("is_maximized", isMaximized());
    globalSettings_.endGroup();
    globalSettings_.setValue("last_open_cd_image_path", lastOpenCdImagePath_);
    event->accept();
}

void MainWindow::on_action_OpenCdImage_triggered()
{
    /*
    auto filePath = QFileDialog::getOpenFileName(
                this,
                "Select CD image",
                lastOpenCdImagePath_,
                "BIN files (*.bin)");
                */
    QString filePath("d:\\Isos\\PSX\\Azure Dreams\\Azure Dreams.bin");
    if (filePath.isEmpty())
    { return; }
    try
    {
        adMemoryHandler_->loadCdImage(filePath);
        lastOpenCdImagePath_ = filePath;
        showVRam();
    }
    catch (QString const& error)
    { QMessageBox::critical(this, "CD image open error", error); }
}


void MainWindow::showVRam()
{
    auto vramImage = adMemoryHandler_->vram().asImage();
    ui_->vramImageLabel->setPixmap(QPixmap::fromImage(vramImage));
    ui_->vramImageLabel->adjustSize();
}

void MainWindow::on_actionSaveAlImages_triggered()
{
    QString const resultDialogTitle("Save all portraits");
    QProgressDialog progressDialog(
                "Saving portraits...",
                "Cancel",
                0,
                SPEAKERS_INFO.size(),
                this);
    QVector<QString> errors;
    for (
         uint32_t speakerIndex = 0;
         speakerIndex < SPEAKERS_INFO.size();
         ++speakerIndex)
    {
        auto const& speakerInfo = SPEAKERS_INFO[speakerIndex];
        try
        {
            auto characterPortraitsData =
                    adMemoryHandler_->readCharacterPortraitsData(
                        speakerInfo.speakerId);
            for (
                 uint32_t portraitVariant = 0;
                 portraitVariant < speakerInfo.variantsNumber;
                 ++portraitVariant)
            {
                auto portraitData =
                        characterPortraitsData[portraitVariant].portraitData;
                auto characterPortraitResource =
                        adMemoryHandler_->loadCharacterPortrait(portraitData);
                auto const& animationFrames =
                        characterPortraitResource.animationFrames;
                for (int frame = 0; frame < animationFrames.size(); ++frame)
                {
                    auto const& animationFrame =
                            characterPortraitResource.animationFrames[frame];
                    auto const& graphicSeries = animationFrame.second;
                    auto fullPortraitImage =
                            adMemoryHandler_->combinePortraitGraphicSeries(
                                graphicSeries,
                                portraitData);
                    QString baseName =
                            QString("%1_variant%2_frame%3")
                            .arg(speakerInfo.name)
                            .arg(portraitVariant)
                            .arg(frame);
                    QString extension(".png");
                    fullPortraitImage.save(baseName + extension);
                    if (graphicSeries.size() > 1)
                    {
                        for (
                             int elementIndex = 0;
                             elementIndex < graphicSeries.size();
                             ++elementIndex)
                        {
                            auto graphicElementImage =
                                    graphicSeries[elementIndex].second;
                            graphicElementImage.save(
                                        QString("%1_element%2%3")
                                        .arg(baseName)
                                        .arg(elementIndex)
                                        .arg(extension));
                        }
                    }
                }
            }
            progressDialog.setValue(speakerIndex);
            if (progressDialog.wasCanceled())
            { return; }
        }
        catch (QString const& error)
        { errors.append(error); }
    }
    if (errors.isEmpty())
    {
        QMessageBox::information(
                    this,
                    resultDialogTitle,
                    "Saving portrait finished successfully.");
    }
    else
    {
        auto it = errors.begin();
        QString errorsString(*it);
        ++it;
        while (it != errors.end())
        {
            errorsString += '\n';
            errorsString += *it;
            ++it;
        }
        QMessageBox::warning(
                    this,
                    resultDialogTitle,
                    "Could not save all portraits.\n" + errorsString);
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
    try
    {
        selectedCharacterPortraitsData_ =
                adMemoryHandler_->readCharacterPortraitsData(
                    speakerInfo.speakerId);
    }
    catch (QString const& error)
    {
        QMessageBox::critical(
                    this,
                    "Character portraits data load error",
                    error);
        return;
    }
    for (
         int variantIndex = 0;
         variantIndex < selectedCharacterPortraitsData_.size();
         ++variantIndex)
    { ui_->variantsListWidget->addItem(QString::number(variantIndex)); }
}

void MainWindow::onVariantSelectionChanged()
{
    ui_->portraitInfoTextEdit->setText("");
    auto selectedItems = ui_->variantsListWidget->selectedItems();
    if (selectedItems.empty())
    {
        onVariantSelectionCleared();
        return;
    }
    clearPortrait();
    try
    {
        auto portraitVariant =
                ui_->variantsListWidget->row(selectedItems.first());
        loadPortraitVariant(portraitVariant);
    }
    catch (QString const& error)
    { QMessageBox::warning(this, "Load portrait", error); }
}

void MainWindow::onVariantSelectionCleared()
{ clearPortrait(); }

void MainWindow::clearPortrait()
{
    auto portraitWidget = ui_->portraitScrollArea->takeWidget();
    if (portraitWidget != nullptr)
    { delete portraitWidget; }
    ui_->portraitInfoTextEdit->setText("");
}

void MainWindow::loadPortraitVariant(int portraitVariant)
{
    auto insertPortraitInfoLine = [=](QString const& text) {
        ui_->portraitInfoTextEdit->insertPlainText(text);
        ui_->portraitInfoTextEdit->insertPlainText("\n");
    };

    if (portraitVariant >= selectedCharacterPortraitsData_.size())
    {
        throw QString(
                    "Selected portrait variant (%1) is greater than number of "
                    "variants (%2)")
                .arg(portraitVariant)
                .arg(selectedCharacterPortraitsData_.size());
        return;
    }
    auto const& characterPortraitData =
            selectedCharacterPortraitsData_[portraitVariant];
    auto* widget = new QWidget(ui_->portraitScrollArea);
    auto createLabel = [=](QString const& text = "") {
        auto* label = new QLabel(text, widget);
        label->setFrameShadow(QFrame::Plain);
        label->setFrameShape(QFrame::Box);
        return label;
    };
    auto* layout = new QGridLayout(widget);
    layout->setSpacing(0);
    widget->setLayout(layout);
    layout->addWidget(createLabel("Frame"), 0, 0);
    layout->addWidget(createLabel("Full image"), 0, 1);
    layout->addWidget(createLabel("Image parts"), 0, 2);
    insertPortraitInfoLine(
                "Portrait data address: " +
                toQString(characterPortraitData.portraitDataAddress));
    auto const& portraitData = characterPortraitData.portraitData;
    insertPortraitInfoLine(
                "Memory load info address: " +
                toQString(portraitData.portraitMemoryLoadInfoAddress));
    insertPortraitInfoLine(
                "Animation address: " +
                toQString(portraitData.animationAddress));
    try
    {
        auto characterPortraitResource =
            adMemoryHandler_->loadCharacterPortrait(portraitData);
        auto const& memoryLoadInfo =
                characterPortraitResource.resourcesMemoryLoadInfo;
        insertPortraitInfoLine("Memory load info:");
        insertPortraitInfoLine(
                    "  Address: " + toQString(memoryLoadInfo.address));
        insertPortraitInfoLine(
                    QString("  Sector: 0x%1")
                    .arg(memoryLoadInfo.sector, 0, 16));
        insertPortraitInfoLine(
                    QString("  Sectors number: 0x%1")
                    .arg(memoryLoadInfo.sectorsNumber, 0, 16));
        insertPortraitInfoLine(
                    QString("Graphics offset halved: %1")
                    .arg(
                        characterPortraitResource.graphicsOffsetHalved ?
                            "true" :
                            "false"));
        auto const& animationFrames = characterPortraitResource.animationFrames;
        for (int frame = 0; frame < animationFrames.size(); ++frame)
        {
            insertPortraitInfoLine(QString("Animation frame %1:").arg(frame));
            auto const& animationFrame = animationFrames[frame];
            auto const& animation = animationFrame.first;
            insertPortraitInfoLine(
                        QString("  Duration: %1")
                        .arg(animation.duration));
            insertPortraitInfoLine(
                        "  Graphic address: " +
                        toQString(animation.graphicAddress));
            auto portraitImage =
                    adMemoryHandler_->combineGraphicSeries(
                        animationFrame.second,
                        characterPortraitResource.graphicsOffsetHalved);
            insertPortraitInfoLine(
                        QString("  Full image size: w=%1, h=%2")
                        .arg(portraitImage.width())
                        .arg(portraitImage.height()));
            auto* frameLabel = createLabel(QString::number(frame));
            frameLabel->setAlignment(Qt::AlignCenter);
            frameLabel->setMargin(5);
            layout->addWidget(frameLabel, frame + 1, 0);
            auto* fullImageLabel = createLabel();
            fullImageLabel->setPixmap(QPixmap::fromImage(portraitImage));
            layout->addWidget(fullImageLabel, frame + 1, 1);
            auto* imagePartsFrame = new QFrame(widget);
            imagePartsFrame->setFrameShadow(QFrame::Plain);
            imagePartsFrame->setFrameShape(QFrame::Box);
            auto* imagePartsLayout = new QHBoxLayout(imagePartsFrame);
            auto const& graphicsSeries = animationFrame.second;
            insertPortraitInfoLine("  Image parts:");
            for (int element = 0; element < graphicsSeries.size(); ++element)
            {
                insertPortraitInfoLine(QString("    Part %1:").arg(element));
                auto const& graphicsSeriesElement = graphicsSeries[element];
                auto const& graphic = graphicsSeriesElement.first;
                auto const& texpage = graphic.texpage;
                insertPortraitInfoLine(
                            "      Flags: " + flagsToQString(graphic));
                insertPortraitInfoLine(
                            QString("      BPP: %1")
                            .arg(toQString(texpage.texpageBpp())));
                insertPortraitInfoLine(
                            QString("      Texture page: 0x%1, 0x%2")
                            .arg(texpage.x, 0, 16)
                            .arg(texpage.y, 0, 16));
                insertPortraitInfoLine(
                            QString("      CLUT coordinates: 0x%1, 0x%2")
                            .arg(graphic.clut.x)
                            .arg(graphic.clut.y));
                insertPortraitInfoLine(
                            QString("      Position: %1 (0x%2), %3 (0x%4)")
                            .arg(graphic.xOffset)
                            .arg(static_cast<uint8_t>(graphic.xOffset), 0, 16)
                            .arg(graphic.yOffset)
                            .arg(static_cast<uint8_t>(graphic.yOffset), 0, 16));
                insertPortraitInfoLine(
                            QString(
                                "      In-texture page bounds: x=0x%1, y=0x%2, "
                                "w=0x%3, h=0x%4")
                            .arg(graphic.xOffsetInTexpage, 0, 16)
                            .arg(graphic.yOffsetInTexpage, 0, 16)
                            .arg(graphic.width, 0, 16)
                            .arg(graphic.height, 0, 16));
                auto vramPortraitRect =
                        VirtualPsxVRam::calculateVRamRect(graphic);
                insertPortraitInfoLine(
                            QString(
                                "      VRam bounds: x=0x%1, y=0x%2, "
                                "w=0x%3, h=0x%4")
                            .arg(vramPortraitRect.x(), 0, 16)
                            .arg(vramPortraitRect.y(), 0, 16)
                            .arg(vramPortraitRect.width(), 0, 16)
                            .arg(vramPortraitRect.height(), 0, 16));
                auto* imagePartLabel = new QLabel(imagePartsFrame);
                imagePartLabel->setPixmap(
                            QPixmap::fromImage(graphicsSeriesElement.second));
                imagePartLabel->setToolTip(QString("Part %1").arg(element));
                imagePartsLayout->addWidget(imagePartLabel);
            }
            imagePartsLayout->addStretch();
            imagePartsFrame->setLayout(imagePartsLayout);
            layout->addWidget(imagePartsFrame, frame + 1, 2);
        }
        ui_->portraitScrollArea->setWidget(widget);
    }
    catch (...)
    {
        delete widget;
        throw;
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

QString MainWindow::flagsToQString(Graphic const& graphic)
{
    QString flagsString;
    GraphicFlags mask{GraphicFlags::None};
    auto appendFlagString = [&](QString const& flagString) {
        if (!flagsString.isEmpty())
        { flagsString += '|'; }
        flagsString += flagString;
    };
    auto processFlag = [&](GraphicFlags flag, QString const& flagString) {
        if (graphic.hasFlags(flag))
        {
            mask = mask | flag;
            appendFlagString(flagString);
        }
    };
    processFlag(GraphicFlags::FlipHorizontally, "FlipHorizontally");
    processFlag(GraphicFlags::FlipVertically, "FlipVertically");
    processFlag(GraphicFlags::PartOfSeries, "PartOfSeries");
    processFlag(GraphicFlags::SeriesEnd, "SeriesEnd");
    auto unprocessedFlags = graphic.flags ^ mask;
    if (unprocessedFlags != GraphicFlags::None)
    {
        appendFlagString(
                    QString("0x%1")
                    .arg(static_cast<uint8_t>(unprocessedFlags)));
    }
    return flagsString;
}

uint8_t* MainWindow::unpack2(uint8_t const* src, uint8_t* dest)
{
  uint16_t uVar2;
  int straightBytesToReadCount;
  uint8_t const* nextSrc;
  uint16_t copyFromDestOffset;
  uint8_t valueRead;
  int index;

  valueRead = 0;
  index = 1;
  do {
    while( true ) {
      //decrease read bit counter
      --index;
      nextSrc = src;
      // if read all bits, read next byte
      if (index == 0) {
        valueRead = *src;
        nextSrc = src + 1;
        index = 8;
      }
      // if LS bit is 1, break the loop
      if ((valueRead & 1) != 0) break;
      // set valueRead to nextBit
      valueRead = valueRead >> 1;

      // copy byte from nextSrc to dest
      // set source to after copied byte
      src = nextSrc + 1;
      *dest = *nextSrc;
      dest = dest + 1;
    }

    //decrease read bit counter and read next bit
    --index;
    valueRead >>= 1;
    // if read all bits, read next byte
    if (index == 0) {
      valueRead = *nextSrc;
      nextSrc = nextSrc + 1;
      index = 8;
    }

    // if LS bit is 0...
    if ((valueRead & 1) == 0) {
      // Header
      // 12 bits of negative offset to copy from dest
      // if next 4 bits are non-zero, that's number of bytes to read - 2
      // else next 8 bits are number of bytes to read - 1
      //uVar2 = (nextSrc[0] << 8) | nextSrc[1];
      uVar2 = qFromBigEndian<uint16_t>(nextSrc);
      // read next bit
      valueRead >>= 1;
      if (uVar2 == 0) {
        return dest;
      }
      // move src by 2 bytes, as 2 bytes were read to set uVar2
      src = nextSrc + 2;
      //if ((nextSrc[1] & 0xf) == 0) {
      if ((uVar2 & 0xf) == 0) {
        straightBytesToReadCount = *src + 1;
        src = nextSrc + 3;
      }
      else {
        straightBytesToReadCount = (uVar2 & 0xf) + 2;
      }
      copyFromDestOffset = (uint32_t)(uVar2 >> 4);
    }
    else { // if LS bit is 1
      // next 2 bits number of bytes to read - 2

      //decrease read bit counter and read next bit
      --index;
      valueRead >>= 1;

      // if read all bits, read next byte
      if (index == 0) {
        valueRead = *nextSrc;
        nextSrc = nextSrc + 1;
        index = 8;
      }

      //decrease read bit counter and read next bit
      --index;
      copyFromDestOffset = valueRead >> 1;
      // if read all bits, read next byte
      if (index == 0) {
        copyFromDestOffset = *nextSrc;
        nextSrc = nextSrc + 1;
        index = 8;
      }

      straightBytesToReadCount = (valueRead & 1) * 2 + (copyFromDestOffset & 1) + 2;

      valueRead = copyFromDestOffset >> 1;
      // next 8 bits is negative offset to copy from dest
      // if offset is 0, it means offset is 0x100
      copyFromDestOffset = *nextSrc;
      src = nextSrc + 1;
      if (copyFromDestOffset == 0) {
        copyFromDestOffset = 0x100;
      }
    }

    // Set copy src address to dest - uVar3
    nextSrc = dest - copyFromDestOffset;

    /*
    if (straightBytesToReadCount > 0)
    {
        std::copy(nextSrc, nextSrc + straightBytesToReadCount, dest);
        nextSrc += straightBytesToReadCount;
        dest += straightBytesToReadCount;
        // TODO: might not be needed
        straightBytesToReadCount = 0;
    }
    */
    for (
         ;
         straightBytesToReadCount > 0;
         --straightBytesToReadCount, ++nextSrc, ++dest)
    { *dest = *nextSrc; }
    /*
    while (straightBytesToReadCount != 0) {
      bVar1 = *nextSrc;
      ++nextSrc;
      --straightBytesToReadCount;
      *dest = bVar1;
      ++dest;
    }
    */

  } while( true );
}
