#include "MainWindow.hpp"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QCoreApplication::setOrganizationName("ADResourcesDumper");
    QCoreApplication::setOrganizationDomain("ad.resources.dumper");
    QCoreApplication::setApplicationName("ADResourcesDumper");
    MainWindow w;
    w.show();
    return a.exec();
}
