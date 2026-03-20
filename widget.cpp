#include "widget.h"
#if __has_include("ui_widget.h")
#include "ui_widget.h"
#elif __has_include("build/UARTStreamParsing_autogen/include/ui_widget.h")
#include "build/UARTStreamParsing_autogen/include/ui_widget.h"
#else
#error "ui_widget.h not found. Run CMake configure/build to generate it."
#endif

Widget::Widget(QWidget *parent) : QWidget(parent), ui(new Ui::Widget) {
    ui->setupUi(this);
}

Widget::~Widget() {
    delete ui;
}
