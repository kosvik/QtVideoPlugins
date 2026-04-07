TEMPLATE = lib
TARGET = videonodeoes

QT += core-private multimedia quick

!debian_build {
    QT += multimedia-private qtmultimediaquicktools-private
}

CONFIG += plugin egl console

HEADERS += \
    oesvideonode.h

SOURCES += \
    oesvideonode.cpp

OTHER_FILES += \
    nodeoes.json


unix {
    target.path = $$[QT_INSTALL_PLUGINS]/video/videonode
    INSTALLS += target
}

debian_build {
    LIBS += -lQt5MultimediaQuick
}

