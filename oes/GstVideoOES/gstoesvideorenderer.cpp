#include <QDebug>
#include <gst/video/gstvideometa.h>
#include <gst/allocators/gstdmabuf.h>
#include <drm/drm_fourcc.h>
#include <qabstractvideosurface.h>
#include <private/qgstutils_p.h>
#include <QGuiApplication>
#include <qpa/qplatformnativeinterface.h>

#include "gstoesvideorenderer.h"


namespace {

int _drm_fourcc_from_info (const GstVideoInfo * info)
{
  GstVideoFormat format = GST_VIDEO_INFO_FORMAT (info);

  // qDebug()<<"Getting DRM fourcc for "<<gst_video_format_to_string (format);

  
  switch (format) {
    case GST_VIDEO_FORMAT_YUY2:
      return DRM_FORMAT_YUYV;

    case GST_VIDEO_FORMAT_YVYU:
      return DRM_FORMAT_YVYU;

    case GST_VIDEO_FORMAT_UYVY:
      return DRM_FORMAT_UYVY;

    case GST_VIDEO_FORMAT_VYUY:
      return DRM_FORMAT_VYUY;

    case GST_VIDEO_FORMAT_AYUV:
    case GST_VIDEO_FORMAT_VUYA:
      return DRM_FORMAT_AYUV;

    case GST_VIDEO_FORMAT_NV12:
      return DRM_FORMAT_NV12;

    case GST_VIDEO_FORMAT_NV21:
      return DRM_FORMAT_NV21;

    case GST_VIDEO_FORMAT_NV16:
      return DRM_FORMAT_NV16;

    case GST_VIDEO_FORMAT_NV61:
      return DRM_FORMAT_NV61;

    case GST_VIDEO_FORMAT_NV24:
      return DRM_FORMAT_NV24;

    case GST_VIDEO_FORMAT_YUV9:
      return DRM_FORMAT_YUV410;

    case GST_VIDEO_FORMAT_YVU9:
      return DRM_FORMAT_YVU410;

    case GST_VIDEO_FORMAT_Y41B:
      return DRM_FORMAT_YUV411;

    case GST_VIDEO_FORMAT_I420:
      return DRM_FORMAT_YUV420;

    case GST_VIDEO_FORMAT_YV12:
      return DRM_FORMAT_YVU420;

    case GST_VIDEO_FORMAT_Y42B:
      return DRM_FORMAT_YUV422;

    case GST_VIDEO_FORMAT_Y444:
      return DRM_FORMAT_YUV444;

    case GST_VIDEO_FORMAT_RGB16:
      return DRM_FORMAT_RGB565;

    case GST_VIDEO_FORMAT_BGR16:
      return DRM_FORMAT_BGR565;

    case GST_VIDEO_FORMAT_RGBA:
      return DRM_FORMAT_ABGR8888;

    case GST_VIDEO_FORMAT_RGBx:
      return DRM_FORMAT_XBGR8888;

    case GST_VIDEO_FORMAT_BGRA:
      return DRM_FORMAT_ARGB8888;

    case GST_VIDEO_FORMAT_BGRx:
      return DRM_FORMAT_XRGB8888;

    case GST_VIDEO_FORMAT_ARGB:
      return DRM_FORMAT_BGRA8888;

    case GST_VIDEO_FORMAT_xRGB:
      return DRM_FORMAT_BGRX8888;

    case GST_VIDEO_FORMAT_ABGR:
      return DRM_FORMAT_RGBA8888;

    case GST_VIDEO_FORMAT_xBGR:
      return DRM_FORMAT_RGBX8888;

    case GST_VIDEO_FORMAT_NV12_10LE40:
      return DRM_FORMAT_NV15;

    default:
      qInfo()<<"Unsupported format for direct DMABuf.";
      return -1;
  }
}

gint
get_egl_stride (const GstVideoInfo * info, gint plane)
{
  const GstVideoFormatInfo *finfo = info->finfo;
  gint stride = info->stride[plane];

  if (!GST_VIDEO_FORMAT_INFO_IS_TILED (finfo))
    return stride;

  return GST_VIDEO_TILE_X_TILES (stride) *
      GST_VIDEO_FORMAT_INFO_TILE_STRIDE (finfo, plane);
}

qint64 setFrameTimeStamps(QVideoFrame *frame, GstBuffer *buffer)
{
    // GStreamer uses nanoseconds, Qt uses microseconds
    qint64 startTime = GST_BUFFER_TIMESTAMP(buffer);
    if (startTime >= 0) {
        frame->setStartTime(startTime/G_GINT64_CONSTANT (1000));

        qint64 duration = GST_BUFFER_DURATION(buffer);
        if (duration >= 0)
            frame->setEndTime((startTime + duration)/G_GINT64_CONSTANT (1000));
    }
    return startTime;
}

bool check_dmabuf_support(GstBuffer *buffer)
{
    GstVideoMeta* vm = gst_buffer_get_video_meta(buffer);
    if (!vm)
    {
        qWarning("GstVideoMeta is null!");
        return false;
    }

    GstMemory *m = gst_buffer_peek_memory (buffer, 0);
    return m && gst_is_dmabuf_memory (m);
}

OESTexture createTextureFromDmaBuf(GstBuffer *buffer, const GstVideoInfo &info,  EGLDisplay dpy, bool verbose)
{
    static const PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES = reinterpret_cast<PFNGLEGLIMAGETARGETTEXTURE2DOESPROC>(eglGetProcAddress("glEGLImageTargetTexture2DOES"));

    //if (!glEGLImageTargetTexture2DOES)
    //    return 0;
    
    GstVideoMeta* vm = gst_buffer_get_video_meta(buffer);
    if (!vm)
    {
        qWarning("GstVideoMeta is null!");
        return 0;
    }

    // sanity check
    if (vm->n_planes > 3)
    {
        qWarning()<<"Incompatible video format: "<<vm->format;
        return 0;
    }
    guint n_mem = gst_buffer_n_memory(buffer);

    if (verbose) {
        qInfo()<<"mem block in buffer: "<<n_mem;
    }

    guint mem_idx, length;
    gsize offset[3];
    gint fd[3];

    for(guint i=0;i<vm->n_planes;i++)
    {
        if (gst_buffer_find_memory (buffer, vm->offset[i], 1, &mem_idx, &length, &offset[i])) {
            GstMemory *m = gst_buffer_peek_memory (buffer, mem_idx);
            if (!gst_is_dmabuf_memory (m)) {
                qWarning("gst_dmabuf_memory_get_fd failed!");
                return 0;
            }
            fd[i] = gst_dmabuf_memory_get_fd (m);
            offset[i] += m->offset;
        } else {
           qWarning("GstBuffer is not valid for the format!");
           return 0;
        }
    }

    bool with_modifiers = false;
    int fourcc = _drm_fourcc_from_info (&info);    

    if (verbose) {
        qInfo()<<"format: "<<GST_VIDEO_INFO_FORMAT (&info)<<" planes: "<<vm->n_planes<<" width: "<<vm->width<<" height: "<<vm->height;
        for(guint i=0;i<vm->n_planes;i++)
        {
           qInfo()<<" plane "<<i<<" fd: "<<fd[i]<<" offset: "<<offset[i]<<" stride: "<<vm->stride[i];
        }
    }

    EGLAttrib attribs[41];         /* 6 + 10 * 3 + 4 + 1 */
    gint atti = 0;
    guint64 modifier = DRM_FORMAT_MOD_BROADCOM_VC4_T_TILED;

    attribs[atti++] = EGL_WIDTH;
    attribs[atti++] = GST_VIDEO_INFO_WIDTH (&info);
    attribs[atti++] = EGL_HEIGHT;
    attribs[atti++] = GST_VIDEO_INFO_HEIGHT (&info);
    attribs[atti++] = EGL_LINUX_DRM_FOURCC_EXT;
    attribs[atti++] = fourcc;

    /* first plane */
    {
      attribs[atti++] = EGL_DMA_BUF_PLANE0_FD_EXT;
      attribs[atti++] = fd[0];
      attribs[atti++] = EGL_DMA_BUF_PLANE0_OFFSET_EXT;
      attribs[atti++] = offset[0];
      attribs[atti++] = EGL_DMA_BUF_PLANE0_PITCH_EXT;
      attribs[atti++] = vm->stride[0]; //get_egl_stride (&info, 0) * stride_scale;
      if (with_modifiers) {
        attribs[atti++] = EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT;
        attribs[atti++] = modifier & 0xffffffff;
        attribs[atti++] = EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT;
        attribs[atti++] = (modifier >> 32) & 0xffffffff;
      }
    }
    /* second plane */
    if (vm->n_planes >= 2) {
      attribs[atti++] = EGL_DMA_BUF_PLANE1_FD_EXT;
      attribs[atti++] = fd[1];
      attribs[atti++] = EGL_DMA_BUF_PLANE1_OFFSET_EXT;
      attribs[atti++] = offset[1];
      attribs[atti++] = EGL_DMA_BUF_PLANE1_PITCH_EXT;
      attribs[atti++] = vm->stride[1];
      if (with_modifiers) {
        attribs[atti++] = EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT;
        attribs[atti++] = modifier & 0xffffffff;
        attribs[atti++] = EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT;
        attribs[atti++] = (modifier >> 32) & 0xffffffff;
      }
    }

    /* third plane */
    if (vm->n_planes == 3) {
      attribs[atti++] = EGL_DMA_BUF_PLANE2_FD_EXT;
      attribs[atti++] = fd[2];
      attribs[atti++] = EGL_DMA_BUF_PLANE2_OFFSET_EXT;
      attribs[atti++] = offset[2];
      attribs[atti++] = EGL_DMA_BUF_PLANE2_PITCH_EXT;
      attribs[atti++] = vm->stride[2];
      if (with_modifiers) {
        attribs[atti++] = EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT;
        attribs[atti++] = modifier & 0xffffffff;
        attribs[atti++] = EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT;
        attribs[atti++] = (modifier >> 32) & 0xffffffff;
      }
    }
#if 1
    {
      uint32_t color_space;
      switch (info.colorimetry.matrix) {
        case GST_VIDEO_COLOR_MATRIX_BT601:
          color_space = EGL_ITU_REC601_EXT;
          break;
        case GST_VIDEO_COLOR_MATRIX_BT709:
          color_space = EGL_ITU_REC709_EXT;
          break;
        case GST_VIDEO_COLOR_MATRIX_BT2020:
          color_space = EGL_ITU_REC2020_EXT;
          break;
        default:
          color_space = 0;
          break;
    }
    if (color_space != 0) {
      attribs[atti++] = EGL_YUV_COLOR_SPACE_HINT_EXT;
      attribs[atti++] = color_space;
    }
  }

  {
    uint32_t range;
    switch (info.colorimetry.range) {
      case GST_VIDEO_COLOR_RANGE_0_255:
        range = EGL_YUV_FULL_RANGE_EXT;
        break;
      case GST_VIDEO_COLOR_RANGE_16_235:
        range = EGL_YUV_NARROW_RANGE_EXT;
        break;
      default:
        range = 0;
        break;
    }
    if (range != 0) {
      attribs[atti++] = EGL_SAMPLE_RANGE_HINT_EXT;
      attribs[atti++] = range;
    }
  }
#endif
  /* Add the EGL_NONE sentinel */
    attribs[atti] = EGL_NONE;

    EGLImageKHR eglimage = eglCreateImage(dpy,EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, attribs);

    if (eglimage == EGL_NO_IMAGE_KHR)
    {
        qWarning("eglCreateImageKHR failed!");
        return 0;
    }

    GLuint tex;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, tex);

    // Import the EGL Image into the texture
    glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, eglimage);

    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    eglDestroyImage(dpy, eglimage);
    return tex;

}

} // namespace

GstOESVideoBuffer::GstOESVideoBuffer(GstBuffer *buffer, const GstVideoInfo &info, EGLDisplay dpy)
    :QAbstractVideoBuffer(OESTextureHandle)
    ,m_buffer(buffer)
    ,m_videoInfo(info)
    ,m_dpy(dpy)
    ,m_texture(0)
{
    // qDebug()<<"GstEgl ref buffer: "<<(void*)m_buffer;
    gst_buffer_ref(m_buffer);
}

GstOESVideoBuffer::~GstOESVideoBuffer()
{
    if (m_texture != 0)
        glDeleteTextures(1, &m_texture);

    // qDebug()<<"GstEgl unref buffer: "<<(void*)m_buffer;
    gst_buffer_unref(m_buffer);

}

QAbstractVideoBuffer::MapMode GstOESVideoBuffer::mapMode() const
{
    return NotMapped;
}

uchar *GstOESVideoBuffer::map(QAbstractVideoBuffer::MapMode mode, int *numBytes, int *bytesPerLine)
{
    Q_UNUSED(mode);
    Q_UNUSED(numBytes);
    Q_UNUSED(bytesPerLine);
    return nullptr;
}

QVariant GstOESVideoBuffer::handle() const
{
    if (m_texture == 0)
    {
        bool verbose = false;
        m_texture = createTextureFromDmaBuf(m_buffer, m_videoInfo, m_dpy, verbose);
        if (m_texture == 0)
        {
            qWarning("Failed to create OES texture from dmabuf!");
        }
    }
    return QVariant::fromValue<OESTexture>(m_texture);
}


GstOESVideoRenderer::GstOESVideoRenderer():m_eglDisplay(EGL_NO_DISPLAY),m_flushed(true),m_verbose(false),m_frameCount(0)
{

}

GstOESVideoRenderer::~GstOESVideoRenderer()
{
}

GstCaps *GstOESVideoRenderer::getCaps(QAbstractVideoSurface *surface)
{
    qInfo()<<"GstOESVideoRenderer:getCaps";

    if (!eglGetProcAddress("glEGLImageTargetTexture2DOES"))
    {
        return gst_caps_new_empty();
    }
#if 1    
    QList<QVideoFrame::PixelFormat> pflist = surface->supportedPixelFormats(OESTextureHandle);
    if (pflist.empty())
    {
        qWarning()<<"No supported pixel formats";
    }
#else
    auto pflist = QList<QVideoFrame::PixelFormat>() 
          << QVideoFrame::Format_ABGR32
          << QVideoFrame::Format_ARGB32;
#endif
    GstCaps *caps = QGstUtils::capsForFormats(pflist);
   // GstCapsFeatures *features = gst_caps_features_new("memory:DMABuf", NULL);

    // 3. Attach the feature to the caps
    // This transforms "video/x-raw" into "video/x-raw(memory:NV12_128L24)"
   // gst_caps_set_features(caps, 0, features);
    return caps;
}

bool GstOESVideoRenderer::start(QAbstractVideoSurface *surface, GstCaps *caps)
{
    
    m_frameCount=0;
    m_verbose = qgetenv("QTGSTEGL_DEBUG").toInt() > 0;
    if (m_eglDisplay == EGL_NO_DISPLAY)
    {
        QPlatformNativeInterface* nativeInf = QGuiApplication::platformNativeInterface();

        m_eglDisplay = static_cast<EGLDisplay>(nativeInf->nativeResourceForIntegration("eglDisplay"));

        if (m_eglDisplay == EGL_NO_DISPLAY)
        {
            qWarning("No EGL Display");
            return false;
        }
    }

    m_flushed = true;
    m_format = QGstUtils::formatForCaps(caps, &m_videoInfo, OESTextureHandle);

    qInfo()<<"GstOESVideoRenderer:start "<<m_videoInfo.width<<"x"<<m_videoInfo.height;

    //m_format = QVideoSurfaceFormat(QSize(m_videoInfo.width,m_videoInfo.height),QVideoFrame::Format_NV21,QAbstractVideoBuffer::EGLImageHandle);

    bool res = m_format.isValid() && surface->start(m_format);
    if (!res)
	    qWarning("start failed");

    if (res)
        m_timer.start();
 
    return res;
}

void GstOESVideoRenderer::stop(QAbstractVideoSurface *surface)
{
#ifndef QT_NO_DEBUG
    qInfo()<<"GstOESVideoRenderer::stopped";
#endif
    m_flushed = true;
    if (surface)
        surface->stop();
    // m_frame = QVideoFrame();
}

bool GstOESVideoRenderer::proposeAllocation(GstQuery *query)
{
    GstCaps *caps;
    gboolean need_pool;
    uint size;

    gst_query_parse_allocation(query, &caps, &need_pool);
    
    // Use the size from the caps if possible
    GstVideoInfo info;
    if (gst_video_info_from_caps(&info, caps)) {
        size = info.size;
    } else {
        size = m_videoInfo.size; // fallback
    }

    // 1. Configure the pool limits
    uint min_buffers = 3;
    uint max_buffers = 5; 

    // 2. Add the pool configuration to the query
    // Passing NULL for the pool tells upstream to create its own pool 
    // but follow these constraints.
    gst_query_add_allocation_pool(query, NULL, size, min_buffers, max_buffers);

    // 3. Keep the Meta API for zero-copy performance
    gst_query_add_allocation_meta(query, GST_VIDEO_META_API_TYPE, NULL);

    return true;
}

bool GstOESVideoRenderer::present(QAbstractVideoSurface *surface, GstBuffer *buffer)
{
    m_flushed = false;

    bool verbose = m_frameCount>0 && m_frameCount<6 && m_verbose;
    
    if (!check_dmabuf_support(buffer))
    {
        if (verbose)
            qWarning()<<"GstBuffer memory is not DMABuf!";
        return false;
    }
    //EGLImageKHR image = create_eglimage(buffer, m_videoInfo, m_eglDisplay, m_verbose);

    QVideoFrame frame(
                new GstOESVideoBuffer(buffer, m_videoInfo, m_eglDisplay),
                m_format.frameSize(),
                m_format.pixelFormat());
    qint64 st =setFrameTimeStamps(&frame, buffer);

    bool res = surface->present(frame);

    if (verbose) {
        qDebug()<<"GstOES presented at:"<<m_timer.elapsed()<<" ts:"<<(st / G_GINT64_CONSTANT (1000))<< " result:"<<res;
    }
    m_timer.restart();

    m_frameCount +=1;
    return res;
}

void GstOESVideoRenderer::flush(QAbstractVideoSurface *surface)
{
    if (surface && !m_flushed)
    {
#ifndef QT_NO_DEBUG
        qWarning()<<"GstOESVideoRenderer::flush "<<m_timer.elapsed();
#endif
       // m_frame = QVideoFrame();
        surface->present(QVideoFrame());
    }
    m_flushed = true;
}

QGstVideoRendererFactory_OES::QGstVideoRendererFactory_OES(QObject *parent) :
    QGstVideoRendererPlugin(parent)
{
}

QGstVideoRenderer *QGstVideoRendererFactory_OES::createRenderer()
{
    qInfo()<<"create GstOESVideoRenderer";
    return new GstOESVideoRenderer();
}
