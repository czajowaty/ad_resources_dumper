#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include "AdDefinitions.hpp"
#include "AdMemoryHandler.hpp"
#include "AdSpeakerId.hpp"
#include "BinCdImageReader.hpp"
#include "VirtualPsxRam.hpp"
#include "VirtualPsxVRam.hpp"
#include <QGraphicsScene>
#include <QMainWindow>
#include <QSettings>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent* event);

private slots:
    void on_action_OpenCdImage_triggered();
    void on_actionSaveAlImages_triggered();
    void onCharacterSelectionChanged();
    void onVariantSelectionChanged();

private:
    void showVRam();
    void onVariantSelectionCleared();
    void clearPortrait();
    void loadPortraitVariant(int portraitVariant);
    QString toQString(PsxRamAddress const& psxRamAddress);
    QString toQString(TexpageBpp bpp);
    QString flagsToQString(Graphic const& graphic);
    uint8_t* unpack2(uint8_t const* src, uint8_t* dest);

    Ui::MainWindow* ui_;
    QSettings globalSettings_;
    QString lastOpenCdImagePath_;
    int selectedSpeakerIndex_{-1};
    std::unique_ptr<AdMemoryHandler> adMemoryHandler_;
    CharacterPortraitsData selectedCharacterPortraitsData_;
};
#endif // MAINWINDOW_HPP
