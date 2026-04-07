// Copyright (C) 2026 Konstantin Yauseyenka.
// SPDX-License-Identifier: LGPL-3.0-only

#ifndef PLANARVIDEONODE_H
#define PLANARVIDEONODE_H

#include <private/qsgvideonode_p.h>

#include <QSGOpaqueTextureMaterial>
#include <QSGTexture>
#include "videobuffer_planar.h"
//#include <EGL/egl.h>
//#include <EGL/eglext.h>

#ifdef Bool
#  undef Bool
#endif
#ifdef None
#  undef None
#endif

QT_BEGIN_NAMESPACE

class QSGVideoMaterial_Planar;

class QSGVideoNode_Planar : public QSGVideoNode
{
public:
    QSGVideoNode_Planar(const QVideoSurfaceFormat &format);
    ~QSGVideoNode_Planar();
    void setCurrentFrame(const QVideoFrame &frame, FrameFlags flags) override;
    QVideoFrame::PixelFormat pixelFormat() const override { return m_pixelFormat; }
    QAbstractVideoBuffer::HandleType handleType() const override  { return PlanarTexturesHandle; }

private:
    QSGVideoMaterial_Planar* m_material;
    QVideoFrame::PixelFormat m_pixelFormat;
//    QVideoFrame m_frame;
};

class QSGVideoNodeFactory_Planar : public QSGVideoNodeFactoryPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.qt.sgvideonodefactory/5.2" FILE "nodeplanar.json")
public:
    QList<QVideoFrame::PixelFormat> supportedPixelFormats(
            QAbstractVideoBuffer::HandleType handleType) const;
    QSGVideoNode *createNode(const QVideoSurfaceFormat &format);
};

QT_END_NAMESPACE

#endif

