#include "qt_main_window.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    Main_window window;
    window.show();

    return application.exec();
}
