#----------------------------------------------------------------
# AuJson - 高性能 JSON 库（原生 C++ 核心 + Qt 适配层）
#   设计文档：docs/AuJson-设计文档.md
#   依赖：核心层零依赖（纯 C++11）；Qt 适配层仅 QtCore
#----------------------------------------------------------------

TEMPLATE = lib
TARGET = AuJson
DEFINES += AUJSON_LIBRARY

# 核心层零 Qt 依赖（qmake 默认 QT = core gui，去掉 gui）；适配层在下方按需加回 core
QT -= gui

CONFIG += c++11
greaterThan(QT_MAJOR_VERSION, 5) {
    CONFIG -= c++11
    CONFIG += c++17
}

DESTDIR = $$PWD/bin
INCLUDEPATH += $$PWD/include

DEFINES += QT_DEPRECATED_WARNINGS

HEADERS += \
    include/AuJson_global.h \
    include/aujson.hpp \
    include/aujson_qt.hpp \
    src/detail/number.hpp

SOURCES += \
    src/value.cpp \
    src/parse.cpp \
    src/stringify.cpp \
    src/path.cpp \
    src/pool.cpp

# Qt 适配层（可选）
qtHaveModule(core) {
    QT += core
    DEFINES += AUJSON_WITH_QT
    SOURCES += src/qt.cpp
}

# MSVC: 源码为 UTF-8 无 BOM，显式指定避免按本地代码页解析中文字符串
msvc: QMAKE_CXXFLAGS += /utf-8
