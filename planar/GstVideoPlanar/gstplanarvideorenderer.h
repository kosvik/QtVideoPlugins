#ifndef GSTPLANARVIDEORENDERER_H
#define GSTPLANARVIDEORENDERER_H

#define MESA_EGL_NO_X11_HEADERS

//#include <qabstractvideobuffer.h>
#include <QtCore/qvariant.h>
#include <QElapsedTimer>
#include "private/qgstvideorendererplugin_p.h"
#include <gst/video/video.h>
#include <EGL/egl.h>
#define EGL_EGLEXT_PROTOTYPES
#include <EGL/eglext.h>
#include <qvideoframe.h>
#include "../VideoNodePlanar/videobuffer_planar.h"

class GstPlanarVideoBuffer : public QAbstractVideoBuffer
{
public:
    GstPlanarVideoBuffer(GstBuffer *buffer, const GstVideoInfo &info, EGLDisplay dpy);

    ~GstPlanarVideoBuffer();

    MapMode mapMode() const override;

    virtual uchar *map(MapMode mode, int *numBytes, int *bytesPerLine) override ;

    void unmap() override {}

    QVariant handle() const override;
  
    
private:

    GstBuffer *m_buffer;

    GstVideoInfo m_videoInfo;

    EGLDisplay m_dpy;

    mutable PlanarTextures m_handle;

};


class GstPlanarVideoRender : public QGstVideoRenderer
{
public:
    GstPlanarVideoRender();
    // ~GstPlanarVideoRender();
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
//    EGLImageKHR m_image;
//  QVideoFrame m_frame;
//#ifndef QT_NO_DEBUG
    QElapsedTimer m_timer;

//#endif
};


class QGstVideoRendererFactory_Planar : public QGstVideoRendererPlugin
{
    Q_OBJECT
//#if QT_VERSION >= 0x050000
    Q_PLUGIN_METADATA(IID QGstVideoRendererInterface_iid FILE "gstplanar.json")
//#endif

public:
    QGstVideoRendererFactory_Planar(QObject *parent = 0);
    QGstVideoRenderer *createRenderer() override;
};

#endif // GSTPLANARVIDEORENDERER_H
