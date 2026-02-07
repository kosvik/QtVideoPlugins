TEMPLATE = lib
TARGET = videonodeplanar

QT += multimedia-private qtmultimediaquicktools-private

CONFIG += plugin egl console

HEADERS += \
    planarvideonode.h

SOURCES += \
    planarvideonode.cpp

OTHER_FILES += \
    nodeplanar.json


unix {
    target.path = $$[QT_INSTALL_PLUGINS]/video/videonode
    INSTALLS += target
}

# LIBS += -lEGL
