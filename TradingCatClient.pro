QT = core network gui charts

TARGET = TradingCatClient
TEMPLATE = app

CONFIG += c++20
CONFIG += static

VERSION = 0.1

HEADERS += \
    $$PWD/Src/localconfig.h \
    $$PWD/Src/mainwindow.h \
    Src/eventlistmenu.h \
    Src/networkcore.h

SOURCES += \
    $$PWD/Src/main.cpp \
    $$PWD/Src/localconfig.cpp \
    $$PWD/Src/mainwindow.cpp \
    Src/eventlistmenu.cpp \
    Src/networkcore.cpp

FORMS += \
    $$PWD/Src/mainwindow.ui \
    Src/eventlistmenu.ui

RESOURCES += \
    $$PWD/Src/resurce.qrc

QMAKE_CXXFLAGS += -oz -flto -fexceptions
QMAKE_LFLAGS += -flto -fexceptions

#QMAKE_CXXFLAGS += \
#    -fwasm-exceptions

#inlude addition library
include($$PWD/../../Common/Common/Common.pri)
include($$PWD/../TradingCatCommon/TradingCatCommon.pri)

RC_ICON = $$PWD/Src/img/sing_cat_icon.ico
