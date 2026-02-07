#include "planarvideonode.h"
#include <QtCore/qmutex.h>
#include <QtMultimedia/qvideosurfaceformat.h>

QT_BEGIN_NAMESPACE
namespace
{

    static QMatrix4x4 colorMatrix(ColorSpace colorSpace, bool colorRangeFull)
    {

        switch (colorSpace)
        {
        default:
            qDebug() << "Unknown color space, defaulting to BT709";
        case ColorSpace::BT709:
            if (colorRangeFull)
                return {
                    1.0f, 0.0f, 1.5748f, -0.790488f,
                    1.0f, -0.187324f, -0.468124f, 0.329010f,
                    1.0f, 1.855600f, 0.0f, -0.931439f,
                    0.0f, 0.0f, 0.0f, 1.0f};
            return {
                1.1644f, 0.0000f, 1.7927f, -0.9729f,
                1.1644f, -0.2132f, -0.5329f, 0.3015f,
                1.1644f, 2.1124f, 0.0000f, -1.1334f,
                0.0000f, 0.0000f, 0.0000f, 1.0000f};
        case ColorSpace::BT2020:
            if (colorRangeFull)
                return {
                    1.f, 0.0000f, 1.4746f, -0.7402f,
                    1.f, -0.1646f, -0.5714f, 0.3694f,
                    1.f, 1.8814f, 0.000f, -0.9445f,
                    0.0f, 0.0000f, 0.000f, 1.0000f};
            return {
                1.1644f, 0.000f, 1.6787f, -0.9157f,
                1.1644f, -0.1874f, -0.6504f, 0.3475f,
                1.1644f, 2.1418f, 0.0000f, -1.1483f,
                0.0000f, 0.0000f, 0.0000f, 1.0000f};
        case ColorSpace::BT601:
            // Corresponds to the primaries used by NTSC BT601. For PAL BT601, we use the BT709 conversion
            // as those are very close.
            if (colorRangeFull)
                return {
                    1.f, 0.000f, 1.772f, -0.886f,
                    1.f, -0.1646f, -0.57135f, 0.36795f,
                    1.f, 1.42f, 0.000f, -0.71f,
                    0.0f, 0.000f, 0.000f, 1.0000f};
            return {
                1.164f, 0.000f, 1.596f, -0.8708f,
                1.164f, -0.392f, -0.813f, 0.5296f,
                1.164f, 2.017f, 0.000f, -1.0810f,
                0.000f, 0.000f, 0.000f, 1.0000f};
        }
    }
}

class QSGVideoMaterial_PlanarShader : public QSGMaterialShader
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
    int id_colorMatrix;
    int id_textures[3];
};

char const *const *QSGVideoMaterial_PlanarShader::attributeNames() const
{
    static char const *const attr[] = {"position", "texcoord", 0};
    return attr;
}

QSGMaterialType QSGVideoMaterial_PlanarShader::type;

void QSGVideoMaterial_PlanarShader::initialize()
{
    id_matrix = program()->uniformLocation("qt_Matrix");
    id_opacity = program()->uniformLocation("opacity");
    id_colorMatrix = program()->uniformLocation("colorMatrix");
    id_textures[0] = program()->uniformLocation("plane1Texture");
    id_textures[1] = program()->uniformLocation("plane2Texture");
    id_textures[2] = program()->uniformLocation("plane3Texture");
}

const char *QSGVideoMaterial_PlanarShader::vertexShader() const
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

const char *QSGVideoMaterial_PlanarShader::fragmentShader() const
{
    return "\n uniform sampler2D plane1Texture;"
           "\n uniform sampler2D plane2Texture;"
           "\n uniform sampler2D plane3Texture;"
           "\n uniform mediump mat4 colorMatrix;"
           "\n uniform lowp float opacity;"
           "\n varying highp vec2 texCoord;"
           "\n void main(void)"
           "\n {"
           "\n     mediump float Y = texture2D(plane1Texture, texCoord).r;"
           "\n     mediump float U = texture2D(plane2Texture, texCoord).r;"
           "\n     mediump float V = texture2D(plane3Texture, texCoord).r;"
           "\n     mediump vec4 color = vec4(Y, U, V, 1.);"
           "\n     gl_FragColor = colorMatrix * color * opacity;"
           "\n }";
}

class QSGVideoMaterial_Planar : public QSGMaterial
{
public:
    QSGVideoMaterial_Planar()
        : m_nextFrame(), m_currentFrame(), m_opacity(1.0)
    {
        setFlag(Blending, false);
    }
    ~QSGVideoMaterial_Planar()
    {
    }

    QSGMaterialShader *createShader() const override
    {
        return new QSGVideoMaterial_PlanarShader;
    }

    QSGMaterialType *type() const override
    {
        return &QSGVideoMaterial_PlanarShader::type;
    }

    int compare(const QSGMaterial *other) const override
    {
        Q_UNUSED(other);
        return 0; // always different
    }

    void updateBlending()
    {
        setFlag(Blending, qFuzzyCompare(m_opacity, qreal(1.0)) ? false : true);
    }

    void setVideoFrame(const QVideoFrame &frame)
    {
        QVideoFrame tmpFrame{};
        {
            QMutexLocker lock(&m_frameMutex);
            tmpFrame = m_nextFrame;
            m_nextFrame = frame;
        }
    }

    void bind()
    {
        QOpenGLFunctions *functions = QOpenGLContext::currentContext()->functions();
        {
            QMutexLocker lock(&m_frameMutex);
            if (m_nextFrame.isValid())
            {
                m_currentFrame = m_nextFrame;
                m_nextFrame = QVideoFrame{};
            }
        }

        if (!m_currentFrame.isValid())
        {
            // No valid frame to process
           // return PlanarTextures{};
           return;
        }

        PlanarTextures yuv = m_currentFrame.handle().value<PlanarTextures>();
        m_colorMatrix = ::colorMatrix(yuv.colorSpace, yuv.colorRangeFull);

        
        functions->glActiveTexture(GL_TEXTURE1);
        functions->glBindTexture(GL_TEXTURE_2D, yuv.texture[1]);
        functions->glActiveTexture(GL_TEXTURE2);
        functions->glBindTexture(GL_TEXTURE_2D, yuv.texture[2]);
        functions->glActiveTexture(GL_TEXTURE0); // Finish with 0 as default texture unit
        functions->glBindTexture(GL_TEXTURE_2D, yuv.texture[0]);
    }
    // void setImage(EGLImageKHR image);
private:
    friend class QSGVideoMaterial_PlanarShader;

    QMatrix4x4 m_colorMatrix;
    QVideoFrame m_nextFrame;
    QVideoFrame m_currentFrame;
    QMutex m_frameMutex;
    qreal m_opacity;
    
};

void QSGVideoMaterial_PlanarShader::updateState(
    const RenderState &state, QSGMaterial *newMaterial, QSGMaterial *oldMaterial)
{
     Q_UNUSED(oldMaterial);

    QSGVideoMaterial_Planar *material = static_cast<QSGVideoMaterial_Planar *>(newMaterial);
    program()->setUniformValue(id_textures[0], 0);
    program()->setUniformValue(id_textures[1], 1);
    program()->setUniformValue(id_textures[2], 2);

    material->bind();
    program()->setUniformValue(id_colorMatrix, material->m_colorMatrix);

    if (state.isOpacityDirty()) {
        material->m_opacity = state.opacity();
        program()->setUniformValue(id_opacity, GLfloat(material->m_opacity));
    }
    if (state.isMatrixDirty())
        program()->setUniformValue(id_matrix, state.combinedMatrix());

}

QSGVideoNode_Planar::QSGVideoNode_Planar(const QVideoSurfaceFormat &format)
    : m_pixelFormat(format.pixelFormat())
{
    setFlag(QSGNode::OwnsMaterial);
    m_material = new QSGVideoMaterial_Planar();
    setMaterial(m_material);
    qInfo() << "QSGVideoNode_Planar";
}

QSGVideoNode_Planar::~QSGVideoNode_Planar()
{
}

void QSGVideoNode_Planar::setCurrentFrame(const QVideoFrame &frame, FrameFlags)
{
    m_material->setVideoFrame(frame);
    markDirty(DirtyMaterial);
}

QList<QVideoFrame::PixelFormat> QSGVideoNodeFactory_Planar::supportedPixelFormats(
    QAbstractVideoBuffer::HandleType handleType) const
{
    if (handleType != PlanarTexturesHandle)
        return QList<QVideoFrame::PixelFormat>();

    return QList<QVideoFrame::PixelFormat>()
           << QVideoFrame::Format_Invalid
           << QVideoFrame::Format_YUV420P
           << QVideoFrame::Format_YV12;
    //           << QVideoFrame::Format_NV12
    //           << QVideoFrame::Format_NV21
}

QSGVideoNode *QSGVideoNodeFactory_Planar::createNode(const QVideoSurfaceFormat &format)
{
    return format.handleType() == PlanarTexturesHandle
               ? new QSGVideoNode_Planar(format)
               : 0;
}

QT_END_NAMESPACE