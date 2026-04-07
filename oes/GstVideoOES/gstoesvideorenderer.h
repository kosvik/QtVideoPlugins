#ifndef GSTOESVIDEORENDERER_H
#define GSTOESVIDEORENDERER_H

#define MESA_EGL_NO_X11_HEADERS

//#include <qabstractvideobuffer.h>
#include <QtCore/qvariant.h>
#include <QElapsedTimer>
#include <private/qgstvideorendererplugin_p.h>
#include <gst/video/video.h>
#include <EGL/egl.h>
// #define EGL_EGLEXT_PROTOTYPES
#include <EGL/eglext.h>
#include <qvideoframe.h>
#include "../VideoNodeOES/videobuffer_oes.h"

class GstOESVideoBuffer : public QAbstractVideoBuffer
{
public:
    GstOESVideoBuffer(GstBuffer *buffer, const GstVideoInfo &info, EGLDisplay dpy);

    ~GstOESVideoBuffer();

    MapMode mapMode() const override;

    virtual uchar *map(MapMode mode, int *numBytes, int *bytesPerLine) override ;

    void unmap() override {}

    QVariant handle() const override;
  
    
private:

    GstBuffer *m_buffer;

    GstVideoInfo m_videoInfo;

    EGLDisplay m_dpy;

    mutable OESTexture m_texture;

};


class GstOESVideoRenderer : public QGstVideoRenderer
{
public:
    GstOESVideoRenderer();
    ~GstOESVideoRenderer();
    GstCaps *getCaps(QAbstractVideoSurface *surface) override;
    bool start(QAbstractVideoSurface *surface, GstCaps *caps) override;
    void stop(QAbstractVideoSurface *surface) override;

    bool proposeAllocation(GstQuery *query) override;

    bool present(QAbstractVideoSurface *surface, GstBuffer *buffer) override;
    void flush(QAbstractVideoSurface *surface) override;

private:
    QVideoSurfaceFormat m_format;
    GstVideoInfo m_videoInfo;
    EGLDisplay m_eglDisplay;
    bool m_flushed;
    bool m_verbose;
    int m_frameCount;

    QElapsedTimer m_timer;

};


class QGstVideoRendererFactory_OES : public QGstVideoRendererPlugin
{
    Q_OBJECT
//#if QT_VERSION >= 0x050000
    Q_PLUGIN_METADATA(IID QGstVideoRendererInterface_iid FILE "gstoes.json")
//#endif

public:
    QGstVideoRendererFactory_OES(QObject *parent = 0);
    QGstVideoRenderer *createRenderer() override;
};

#endif // GSTOESVIDEORENDERER_H