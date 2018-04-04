#include "qt_main_window.h"
#include "ui_main_window.h"

Main_window::Main_window(QWidget *parent) : QMainWindow(parent), ui(new Ui::Main_window)
{
    ui->setupUi(this);
}

Main_window::~Main_window()
{
    delete ui;
}
