#if !defined(VIDEOBUFFER_OES_H)
#define VIDEOBUFFER_OES_H

#include <QAbstractVideoBuffer>
#include <QMetaType>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>



const QAbstractVideoBuffer::HandleType OESTextureHandle = static_cast<QAbstractVideoBuffer::HandleType>(QAbstractVideoBuffer::UserHandle + 1);


typedef GLuint OESTexture;


#endif