// Copyright (C) 2026 Konstantin Yauseyenka.
// SPDX-License-Identifier: LGPL-3.0-only

#include "oesvideonode.h"
#include <QtCore/qmutex.h>
#include <QtMultimedia/qvideosurfaceformat.h>

QT_BEGIN_NAMESPACE

class QSGVideoMaterial_OESShader : public QSGMaterialShader
{
public:
    void updateState(const RenderState &state, QSGMaterial *newEffect, QSGMaterial *oldEffect);
    char const *const *attributeNames() const;

    static QSGMaterialType type;

protected:
    void initialize();

    const char *vertexShader() const override;
    const char *fragmentShader() const override;

private:
    int id_matrix;
    int id_opacity;
    int id_texture;
};

char const *const *QSGVideoMaterial_OESShader::attributeNames() const
{
    static char const *const attr[] = {"position", "texcoord", 0};
    return attr;
}

QSGMaterialType QSGVideoMaterial_OESShader::type;

void QSGVideoMaterial_OESShader::initialize()
{
    id_matrix = program()->uniformLocation("qt_Matrix");
    id_opacity = program()->uniformLocation("opacity");
    id_texture = program()->uniformLocation("oesTexture");
}

const char *QSGVideoMaterial_OESShader::vertexShader() const
{
    return "\n uniform highp mat4 qt_Matrix;"
           "\n attribute highp vec4 qt_VertexPosition;"
           "\n attribute highp vec2 qt_VertexTexCoord;"
           "\n varying highp vec2 texCoord;"
           "\n void main(void)"
           "\n {"
           "\n     gl_Position = qt_Matrix * qt_VertexPosition;"
           "\n     texCoord = qt_VertexTexCoord;"
           "\n }";
}

const char *QSGVideoMaterial_OESShader::fragmentShader() const
{
    return "\n #extension GL_OES_EGL_image_external : require"
           "\n uniform samplerExternalOES oesTexture;"
           "\n uniform lowp float opacity;"
           "\n varying highp vec2 texCoord;"
           "\n void main(void)"
           "\n {"
           "\n     gl_FragColor = texture2D(oesTexture, texCoord);"
           "\n }";
}

class QSGVideoMaterial_OES : public QSGMaterial
{
public:
    QSGVideoMaterial_OES()
        : m_nextFrame(), m_currentFrame(), m_opacity(1.0)
    {
        m_forceBlending = qgetenv("QTVIDEONODE_FORCE_BLENDING").toInt() > 0;
        setFlag(Blending, m_forceBlending);
    }
    ~QSGVideoMaterial_OES()
    {
    }

    QSGMaterialShader *createShader() const override
    {
        return new QSGVideoMaterial_OESShader;
    }

    QSGMaterialType *type() const override
    {
        return &QSGVideoMaterial_OESShader::type;
    }

    int compare(const QSGMaterial *other) const override
    {
        Q_UNUSED(other);
        return 0; // always different
    }

    void updateBlending()
    {
        setFlag(Blending, m_forceBlending || !qFuzzyCompare(m_opacity, qreal(1.0)));
    }

    void setVideoFrame(const QVideoFrame &frame)
    {
        m_nextFrame = frame;
    }

    void bind()
    {
        QOpenGLFunctions *functions = QOpenGLContext::currentContext()->functions();

        m_prevFrame = m_currentFrame;

        m_currentFrame = m_nextFrame;

        if (!m_currentFrame.isValid())
        {
            // No valid frame to process
           // return PlanarTextures{};
           return;
        }

        int tex = m_currentFrame.handle().value<OESTexture>();

        
        functions->glActiveTexture(GL_TEXTURE0); // Finish with 0 as default texture unit
        functions->glBindTexture(GL_TEXTURE_EXTERNAL_OES, tex);
    }
    // void setImage(EGLImageKHR image);
private:
    friend class QSGVideoMaterial_OESShader;

    QVideoFrame m_nextFrame;
    QVideoFrame m_currentFrame;
    QVideoFrame m_prevFrame;
    //QMutex m_frameMutex;
    qreal m_opacity;
    bool m_forceBlending;
    
};

void QSGVideoMaterial_OESShader::updateState(
    const RenderState &state, QSGMaterial *newMaterial, QSGMaterial *oldMaterial)
{
     Q_UNUSED(oldMaterial);

    QSGVideoMaterial_OES *material = static_cast<QSGVideoMaterial_OES *>(newMaterial);
    program()->setUniformValue(id_texture, 0);

    material->bind();

    if (state.isOpacityDirty()) {
        material->m_opacity = state.opacity();
        program()->setUniformValue(id_opacity, GLfloat(material->m_opacity));
    }
    if (state.isMatrixDirty())
        program()->setUniformValue(id_matrix, state.combinedMatrix());

}

QSGVideoNode_OES::QSGVideoNode_OES(const QVideoSurfaceFormat &format)
    : m_pixelFormat(format.pixelFormat())
{
    setFlag(QSGNode::OwnsMaterial);
    m_material = new QSGVideoMaterial_OES();
    setMaterial(m_material);
    qInfo() << "QSGVideoNode_OES";
}

QSGVideoNode_OES::~QSGVideoNode_OES()
{
}

void QSGVideoNode_OES::setCurrentFrame(const QVideoFrame &frame, FrameFlags)
{
    m_material->setVideoFrame(frame);
    markDirty(DirtyMaterial);
}

QList<QVideoFrame::PixelFormat> QSGVideoNodeFactory_OES::supportedPixelFormats(
    QAbstractVideoBuffer::HandleType handleType) const
{
    qInfo() << "QSGVideoNodeFactory_OES::supportedPixelFormats handleType=" << handleType;
    if (handleType != OESTextureHandle)
        return QList<QVideoFrame::PixelFormat>();

    return QList<QVideoFrame::PixelFormat>()
           << QVideoFrame::Format_NV12
           << QVideoFrame::Format_YUV420P
           << QVideoFrame::Format_YV12
           << QVideoFrame::Format_NV21
           << QVideoFrame::Format_YUYV
           << QVideoFrame::Format_RGB32
           << QVideoFrame::Format_BGR32
           << QVideoFrame::Format_RGB24
           << QVideoFrame::Format_BGR24
           << QVideoFrame::Format_RGB565
           << QVideoFrame::Format_BGR565;
}

QSGVideoNode *QSGVideoNodeFactory_OES::createNode(const QVideoSurfaceFormat &format)
{
    return format.handleType() == OESTextureHandle
               ? new QSGVideoNode_OES(format)
               : 0;
}

QT_END_NAMESPACE