QT += core gui qml quick quickcontrols2 dbus
CONFIG += c++17 release
TARGET = jot
TEMPLATE = app
HEADERS += src/backend.h src/theme.h src/translator.h src/cli.h
SOURCES += src/main.cpp src/backend.cpp src/theme.cpp src/translator.cpp src/cli.cpp
RESOURCES += src/resources.qrc
