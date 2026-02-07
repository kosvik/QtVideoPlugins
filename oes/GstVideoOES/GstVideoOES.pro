TEMPLATE = lib
TARGET = gstvideooes

QT += core-private multimedia-private multimediagsttools-private 

CONFIG += plugin egl console

QMAKE_USE += gstreamer

HEADERS += \
    gstoesvideorenderer.h

SOURCES += \
    gstoesvideorenderer.cpp

OTHER_FILES += \
    gstoes.json

unix {
    target.path = $$[QT_INSTALL_PLUGINS]/video/gstvideorenderer
    INSTALLS += target
}

# LIBS += -lgstallocators-1.0 -lEGL
