// Copyright (C) 2026 Konstantin Yauseyenka.
// SPDX-License-Identifier: LGPL-3.0-only

#ifndef OESVIDEONODE_H
#define OESVIDEONODE_H

#include <private/qsgvideonode_p.h>

#include <QSGMaterial>
//#include <QSGTexture>
#include "videobuffer_oes.h"
//#include <EGL/egl.h>
//#include <EGL/eglext.h>

#ifdef Bool
#  undef Bool
#endif
#ifdef None
#  undef None
#endif

QT_BEGIN_NAMESPACE

class QSGVideoMaterial_OES;

class QSGVideoNode_OES : public QSGVideoNode
{
public:
    QSGVideoNode_OES(const QVideoSurfaceFormat &format);
    ~QSGVideoNode_OES();
    void setCurrentFrame(const QVideoFrame &frame, FrameFlags flags) override;
    QVideoFrame::PixelFormat pixelFormat() const override { return m_pixelFormat; }
    QAbstractVideoBuffer::HandleType handleType() const override  { return OESTextureHandle; }

private:
    QSGVideoMaterial_OES* m_material;
    QVideoFrame::PixelFormat m_pixelFormat;
//    QVideoFrame m_frame;
};

class QSGVideoNodeFactory_OES : public QSGVideoNodeFactoryPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.qt.sgvideonodefactory/5.2" FILE "nodeoes.json")
public:
    QList<QVideoFrame::PixelFormat> supportedPixelFormats(
            QAbstractVideoBuffer::HandleType handleType) const;
    QSGVideoNode *createNode(const QVideoSurfaceFormat &format);
};

QT_END_NAMESPACE

#endif

