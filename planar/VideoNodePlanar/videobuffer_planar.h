// Copyright (C) 2026 Konstantin Yauseyenka.
// SPDX-License-Identifier: LGPL-3.0-only

#if !defined(VIDEOBUFFER_PLANAR_H)
#define VIDEOBUFFER_PLANAR_H

#include <QAbstractVideoBuffer>
#include <QMetaType>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>



const QAbstractVideoBuffer::HandleType PlanarTexturesHandle = static_cast<QAbstractVideoBuffer::HandleType>(QAbstractVideoBuffer::UserHandle + 3);


enum class ColorSpace {
    Unknown,
    BT601,
    BT709,
    BT2020
};

struct PlanarTextures {
    int numPlanes;        // Number of planes (e.g., 3 for YUV)
    GLuint texture[3];   // OpenGL texture handle for Y, U, V planes
    ColorSpace colorSpace; // Color space of the video frame
    bool colorRangeFull; // true if full range, false if limited range
#if 0    
    int drmFormat;    // DRM format identifier
    int yStride;          // Stride (line size) for Y plane
    int uStride;          // Stride (line size) for U plane
    int vStride;          // Stride (line size) for V plane
#endif    
};

Q_DECLARE_METATYPE(PlanarTextures)

#endif