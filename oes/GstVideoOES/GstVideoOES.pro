TEMPLATE = lib
TARGET = gstvideooes

QT += gui-private multimedia

!debian_build {
    QT += multimedia-private multimediagsttools-private
}

CONFIG += plugin egl console link_pkgconfig c++17

PKGCONFIG += gstreamer-1.0 \
             gstreamer-video-1.0 \
             gstreamer-allocators-1.0 \
             libdrm

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
debian_build {
    LIBS += -lQt5MultimediaGstTools
}

