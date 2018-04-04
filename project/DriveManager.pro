#-------------------------------------------------
#
# Project created by QtCreator 2018-04-04T12:51:41
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = DriveManager
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

ROOT = $$_PRO_FILE_PWD_/..

INCLUDEPATH += $$ROOT/code/

#DESTDIR = out/

#OBJECTS_DIR = _PRO_FILE_PWD_/build/

VPATH += $$ROOT/code/

SOURCES += \
        qt_main.cpp \
        qt_main_window.cpp \
    ../code/main.cpp \
    ../code/stdafx.cpp

HEADERS += \
        qt_main_window.h \
    ../code/app_events.h \
    ../code/console.h \
    ../code/dir_data_view.h \
    ../code/fae_lib.h \
    ../code/fae_memory.h \
    ../code/fae_string.h \
    ../code/misc.h \
    ../code/new_filesystem.h \
    ../code/queue_display.h \
    ../code/settings.h \
    ../code/stdafx.h

FORMS += \
        qt_main_window.ui
