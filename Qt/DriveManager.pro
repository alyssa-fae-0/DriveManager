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

PRECOMPILED_HEADER = external_includes.h

#INCLUDEPATH += $$_PRO_FILE_PWD/code/

#DESTDIR = out/

#OBJECTS_DIR = _PRO_FILE_PWD_/build/

#VPATH += $$ROOT/code/

#LZZ = main_window.lzz

#lzz_cpp.output  = lzz/${QMAKE_FILE_BASE}.cpp
#lzz_cpp.commands = lzz ${_PRO_FILE_PWD_}/${QMAKE_FILE_NAME}
#lzz_cpp.dependency_type = TYPE_CPP
#lzz_cpp.input = LZZ
#lzz_cpp.variable_out = SOURCES
#QMAKE_EXTRA_COMPILERS += lzz_cpp

#lzz_h.output  = lzz/${QMAKE_FILE_BASE}.h
#lzz_h.commands = lzz ${_PRO_FILE_PWD_}/${QMAKE_FILE_NAME}
#lzz_h.dependency_type = TYPE_H
#lzz_h.input = LZZ
#lzz_h.variable_out = HEADERS
#QMAKE_EXTRA_COMPILERS += lzz_h

DEFINES += UNICODE _UNICODE

SOURCES += main.cpp


HEADERS += \
    fae_lib.h \
    fae_memory.h \
    fae_string.h \
    settings.h \
    external_includes.h \
    filesystem.h \
    console.h \
    main_window.h

FORMS += \
    main_window.ui
