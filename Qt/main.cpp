#include "main_window.h"
#include <QApplication>
#include "fae_lib.h"
#include "fae_memory.h"
#include "fae_string.h"
#include "settings.h"
#include <QDir>
#include <QDebug>
#include <QFileSystemModel>
#include <QTreeView>
#include "filesystem.h"

App_Settings Settings;

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    Main_window window;
    window.show();


//    qDebug("Hello World");

//    qDebug() << "HELLO" << endl;

//    QDir dir;
//    if(!dir.setCurrent(u8"C:\\dev\\test_data_source\\"))
//        qDebug() << "Failed to set cwd" << endl;
//    dir.setFilter(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot);
//    //dir.setSorting(QDir::Size | QDir::Reversed);

//    QFileInfoList list = dir.entryInfoList();
//    qDebug() << "     Bytes Filename" << endl;
//    for (int i = 0; i < list.size(); ++i) {
//        QFileInfo fileInfo = list.at(i);
//        qDebug() << qPrintable(
//                        QString("%1 %2").arg(fileInfo.canonicalFilePath()).arg(fileInfo.fileName()));
//        qDebug() << endl;
//    }


    return application.exec();
}
