#include <QDebug>
#include <gst/video/gstvideometa.h>
#include <gst/allocators/gstdmabuf.h>
#include <drm/drm_fourcc.h>
#include <qabstractvideosurface.h>
#include <private/qgstutils_p.h>
#include <QGuiApplication>
#include <qpa/qplatformnativeinterface.h>
#include <EGL/egl.h>

#include "gstplanarvideorenderer.h"

namespace
{

    ColorSpace colorSpaceFromGstVideoColorMatrix(GstVideoColorMatrix colometry)
    {
        switch (colometry)
        {
        case GST_VIDEO_COLOR_MATRIX_BT709:
            return ColorSpace::BT709;
        case GST_VIDEO_COLOR_MATRIX_BT601:
            return ColorSpace::BT601;
        case GST_VIDEO_COLOR_MATRIX_BT2020:
            return ColorSpace::BT2020;
        default:
            qWarning() << "Unsupported color matrix" << colometry;
            return ColorSpace::Unknown;
        }
    }
#if 0
    int
    get_egl_stride(const GstVideoInfo *info, gint plane)
    {
        const GstVideoFormatInfo *finfo = info->finfo;
        gint stride = info->stride[plane];

        if (!GST_VIDEO_FORMAT_INFO_IS_TILED(finfo))
            return stride;

        return GST_VIDEO_TILE_X_TILES(stride) *
               GST_VIDEO_FORMAT_INFO_TILE_STRIDE(finfo, plane);
    }
#endif
    qint64 setFrameTimeStamps(QVideoFrame *frame, GstBuffer *buffer)
    {
        // GStreamer uses nanoseconds, Qt uses microseconds
        qint64 startTime = GST_BUFFER_TIMESTAMP(buffer);
        if (startTime >= 0)
        {
            frame->setStartTime(startTime / G_GINT64_CONSTANT(1000));

            qint64 duration = GST_BUFFER_DURATION(buffer);
            if (duration >= 0)
                frame->setEndTime((startTime + duration) / G_GINT64_CONSTANT(1000));
        }
        return startTime;
    }

    GLuint createTextureFromDmaBuf(EGLDisplay dpy, int fd, int width, int height, int offset, int stride)
    {
        static const PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES = reinterpret_cast<PFNGLEGLIMAGETARGETTEXTURE2DOESPROC>(eglGetProcAddress("glEGLImageTargetTexture2DOES"));

        if (!glEGLImageTargetTexture2DOES)
            return 0;

        EGLAttrib attribs[] = {
            EGL_WIDTH, width,
            EGL_HEIGHT, height,
            EGL_LINUX_DRM_FOURCC_EXT, DRM_FORMAT_R8, // Each plane is 8-bit grayscale
            EGL_DMA_BUF_PLANE0_FD_EXT, fd,
            EGL_DMA_BUF_PLANE0_OFFSET_EXT, offset,
            EGL_DMA_BUF_PLANE0_PITCH_EXT, stride,
            EGL_NONE};

        EGLImageKHR img = eglCreateImage(dpy,
                                            EGL_NO_CONTEXT,
                                            EGL_LINUX_DMA_BUF_EXT,
                                            NULL, attribs);

        if (img == EGL_NO_IMAGE_KHR)
        {
            qWarning("eglCreateImageKHR failed!");
            return 0;
        }

        GLuint tex;

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);

        // Import the EGL Image into the texture
        glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, img);

        // Essential parameters we discussed
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        //glGenerateMipmap(GL_TEXTURE_2D); 
        eglDestroyImage(dpy, img);

        return tex;
    }


    void resetPlanarTextures(PlanarTextures &handle)
    {
        for (int i = 0; i < handle.numPlanes; ++i)
        {
            if (handle.texture[i] != 0)
            {
                glDeleteTextures(1, &handle.texture[i]);
                handle.texture[i] = 0;
            }
        }
        handle.numPlanes = 0;
    }
} // namespace

GstPlanarVideoBuffer::GstPlanarVideoBuffer(GstBuffer *buffer, const GstVideoInfo &info, EGLDisplay dpy)
    : QAbstractVideoBuffer(PlanarTexturesHandle), m_buffer(buffer), m_videoInfo(info), m_dpy(dpy), m_handle()
{
    gst_buffer_ref(m_buffer);
}

GstPlanarVideoBuffer::~GstPlanarVideoBuffer()
{
    resetPlanarTextures(m_handle);
    gst_buffer_unref(m_buffer);
}


QAbstractVideoBuffer::MapMode GstPlanarVideoBuffer::mapMode() const
{
    return NotMapped;
}

uchar *GstPlanarVideoBuffer::map(QAbstractVideoBuffer::MapMode mode, int *numBytes, int *bytesPerLine)
{
    Q_UNUSED(mode);
    Q_UNUSED(numBytes);
    Q_UNUSED(bytesPerLine);
    return nullptr;
}

QVariant GstPlanarVideoBuffer::handle() const
{

    if (m_handle.numPlanes == 0)
    {

        GstVideoMeta *vm = gst_buffer_get_video_meta(m_buffer);
        if (!vm)
        {
            qWarning("GstVideoMeta is null!");

            return QVariant();
        }

        if (vm->n_planes != 3)
        {
            qWarning() << "Incompatible video format: " << vm->format;
            return QVariant();
        }

        // Create textures for each plane

        for (guint i = 0; i < vm->n_planes; i++)
        {
            gsize offset = 0;
            guint mem_idx = 0, length = 0;
            int planeWidth = GST_VIDEO_INFO_COMP_WIDTH(&m_videoInfo, i);
            int planeHeight = GST_VIDEO_INFO_COMP_HEIGHT(&m_videoInfo, i);

            if (gst_buffer_find_memory(m_buffer, vm->offset[i], 1, &mem_idx, &length, &offset))
            {
                GstMemory *m = gst_buffer_peek_memory(m_buffer, mem_idx);
                if (!gst_is_dmabuf_memory(m))
                {
                    qWarning("gst_dmabuf_memory_get_fd failed!");
                    break;
                }
                int fd = gst_dmabuf_memory_get_fd(m);
                m_handle.texture[i] = createTextureFromDmaBuf(m_dpy, fd, planeWidth, planeHeight, offset + m->offset, vm->stride[i]);
                if (m_handle.texture[i] == 0)
                {
                    qWarning("Failed to create texture from dmabuf!");
                    break;
                }
#if 0
                if (i==0)
                {
                    int minf, magf = 0;
                    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, reinterpret_cast<GLint *>(&minf));
                    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, reinterpret_cast<GLint *>(&magf));
                    qDebug() << "Texture plane" << i << " created. MinFilter:" << minf << " MagFilter:" << magf;
                }
#endif
                m_handle.numPlanes += 1;
            }
            else
            {
                qWarning("GstBuffer is not valid for the format!");
                break;
            }
        }

        if (m_handle.numPlanes != 3)
        {
            qWarning() << "Failed to create textures for all planes!";
            resetPlanarTextures(m_handle);
            return QVariant();
        }

            m_handle.colorSpace = colorSpaceFromGstVideoColorMatrix(m_videoInfo.colorimetry.matrix);
            m_handle.colorRangeFull = (m_videoInfo.colorimetry.range == GST_VIDEO_COLOR_RANGE_0_255);
    }

    return QVariant::fromValue<PlanarTextures>(m_handle);
}

GstPlanarVideoRender::GstPlanarVideoRender()
    : m_eglDisplay(EGL_NO_DISPLAY), m_flushed(true), m_verbose(false), m_frameCount(0)
{
}

GstCaps *GstPlanarVideoRender::getCaps(QAbstractVideoSurface *surface)
{
    qInfo() << "GstPlanarVideoRender:getCaps";
    QList<QVideoFrame::PixelFormat> pflist = surface->supportedPixelFormats(PlanarTexturesHandle);
    if (pflist.empty())
    {
        qWarning() << "No supported pixel formats";
    }
    //  pflist << QVideoFrame::Format_NV12;

    return QGstUtils::capsForFormats(pflist);
}

bool GstPlanarVideoRender::start(QAbstractVideoSurface *surface, GstCaps *caps)
{

    m_frameCount = 0;
    m_verbose = qgetenv("QTGSTEGL_DEBUG").toInt() > 0;
    if (m_eglDisplay == EGL_NO_DISPLAY)
    {
        QPlatformNativeInterface *nativeInf = QGuiApplication::platformNativeInterface();

        m_eglDisplay = static_cast<EGLDisplay>(nativeInf->nativeResourceForIntegration("eglDisplay"));

        if (m_eglDisplay == EGL_NO_DISPLAY)
        {
            qWarning("No EGL Display");
            return false;
        }
    }

    m_flushed = true;
    m_format = QGstUtils::formatForCaps(caps, &m_videoInfo, PlanarTexturesHandle);

    qInfo() << "GstPlanarVideoRender:start " << m_videoInfo.width << "x" << m_videoInfo.height << " type:" << (int)m_format.handleType();

    bool res = m_format.isValid() && surface->start(m_format);
    if (!res)
        qWarning("start failed");
    else
        m_timer.start();

    return res;
}

void GstPlanarVideoRender::stop(QAbstractVideoSurface *surface)
{
#ifndef QT_NO_DEBUG
    qWarning() << "GstPlanarVideoRender::stopped";
#endif
    m_flushed = true;
    if (surface)
        surface->stop();
    // m_frame = QVideoFrame();
}

bool GstPlanarVideoRender::proposeAllocation(GstQuery *query)
{
    // GstCaps *caps = NULL;
    // gboolean need_pool;

    // gst_query_parse_allocation (query, &caps, &need_pool);
    // gst_query_add_allocation_pool(query,NULL,m_videoInfo.size,2,0);
    gst_query_add_allocation_meta(query, GST_VIDEO_META_API_TYPE, NULL);

    return true;
}

bool GstPlanarVideoRender::present(QAbstractVideoSurface *surface, GstBuffer *buffer)
{
    m_flushed = false;

    QVideoFrame frame(
        new GstPlanarVideoBuffer(buffer, m_videoInfo, m_eglDisplay),
        m_format.frameSize(),
        m_format.pixelFormat());
    qint64 st = setFrameTimeStamps(&frame, buffer);

    bool res = surface->present(frame);

    if (m_verbose && res)
    {
        qDebug() << "GstPlanarVideoRender presented et:" << m_timer.elapsed() << " ts:" << (st / G_GINT64_CONSTANT(1000));
    }
    // m_frame = frame;
    m_timer.restart();

    m_frameCount += 1;
    return res;
}

void GstPlanarVideoRender::flush(QAbstractVideoSurface *surface)
{
    if (surface && !m_flushed)
    {
#ifndef QT_NO_DEBUG
        qInfo() << "GstPlanarVideoRender::flush " << m_timer.elapsed();
#endif
        // m_frame = QVideoFrame();
        surface->present(QVideoFrame());
    }
    m_flushed = true;
}

QGstVideoRendererFactory_Planar::QGstVideoRendererFactory_Planar(QObject *parent) : QGstVideoRendererPlugin(parent)
{
}

QGstVideoRenderer *QGstVideoRendererFactory_Planar::createRenderer()
{
    qInfo() << "create GstPlanarVideoRender";
    return new GstPlanarVideoRender();
}
