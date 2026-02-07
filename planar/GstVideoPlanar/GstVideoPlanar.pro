TEMPLATE = lib
TARGET = gstvideoplanar

QT += core-private multimedia-private multimediagsttools-private 

CONFIG += plugin egl console

QMAKE_USE += gstreamer

HEADERS += \
    gstplanarvideorenderer.h

SOURCES += \
    gstplanarvideorenderer.cpp

OTHER_FILES += \
    gstplanar.json

unix {
    target.path = $$[QT_INSTALL_PLUGINS]/video/gstvideorenderer
    INSTALLS += target
}

# LIBS += -lgstallocators-1.0 -lEGL
