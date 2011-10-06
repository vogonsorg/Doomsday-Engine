# The Doomsday Engine Project
# Copyright (c) 2011 Jaakko Keränen <jaakko.keranen@iki.fi>

TEMPLATE = lib
TARGET = deng2

# Build Configuration --------------------------------------------------------

include(../config.pri)
VERSION = $$DENG2_VERSION
DEFINES += __DENG2__

# Using Qt.
QT += core network

win32 {
    # Keep the version number out of the file name.
    TARGET_EXT = .dll
}

*-g++ {
    # Enable strict warnings.
    QMAKE_CXXFLAGS_WARN_ON *= -Wall -Wextra -pedantic -Wno-long-long
}

INCLUDEPATH += include

# Source Files ---------------------------------------------------------------

include(data.pri)
include(legacy.pri)
include(network.pri)

# Convenience headers.
HEADERS += \
    include/de/Error \
    include/de/Log \
    include/de/LogBuffer \
    include/de/TextStyle

HEADERS += \
    include/de/c_wrapper.h \
    include/de/error.h \
    include/de/libdeng2.h \
    include/de/core/log.h \
    include/de/core/logbuffer.h \
    include/de/core/textstyle.h

SOURCES += \
    src/c_wrapper.cpp \
    src/core/log.cpp \
    src/core/logbuffer.cpp

# Installation ---------------------------------------------------------------

!macx {
    INSTALLS += target
    target.path = $$DENG_LIB_DIR
}
