#include "widget.h"

#include <QApplication>
#include <QFont>
#include <QFontDatabase>

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);

    // Load embedded HarmonyOS Sans SC font
    int fontId =
        QFontDatabase::addApplicationFont(":/fonts/HarmonyOS_Sans_SC/HarmonyOS_Sans_SC_Regular.ttf");
    QFontDatabase::addApplicationFont(":/fonts/HarmonyOS_Sans_SC/HarmonyOS_Sans_SC_Medium.ttf");
    QFontDatabase::addApplicationFont(":/fonts/HarmonyOS_Sans_SC/HarmonyOS_Sans_SC_Bold.ttf");

    if (fontId != -1) {
        QString family = QFontDatabase::applicationFontFamilies(fontId).at(0);
        QFont appFont(family, 9);
        a.setFont(appFont);
    }

    Widget w;
    w.show();
    return a.exec();
}
