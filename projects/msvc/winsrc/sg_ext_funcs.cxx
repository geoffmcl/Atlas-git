// sg_ext_funcs.cxx
/* --------------------------------------------------------------
   SimGear exrtension support
   From:   Erik Hofman <erik@ehofman.com>
   Reply-To:   FlightGear developers discussions <flightgear-devel@lists.sourceforge.net>
   To:     FlightGear developers discussions <flightgear-devel@lists.sourceforge.net>
   Subject:    Re: [Flightgear-devel] OpenGL EXT Functions - native windows
   Date:   Mon, 03 May 2010 08:57:10 +0200

    There still is support for it in SimGear, see simgear/screen/extensions.hxx

    It works like this:
    1. You need to add the extension support to extensions.hxx (see the
       attached diff for EXT_framebuffer_object support, this has been
       committed to CVS already by the way).

    2. Define the function pointers like this:

        glGenRenderbuffersProc pglGenRenderbuffers;
        glBindFramebufferProc pglBindFramebuffer;
        bool framebuffer_object_support = false;

    3. in the code test if the extension is supported:

    if (SGIsOpenGLExtensionSupported("GL_EXT_framebuffer_object"))
    {
       pglGenRenderbuffers = SGGetGLProcAddress("glGenRenderbuffersEXT");
       pglBindFramebuffer = SGGetGLProcAddress("glBindFramebufferEXT");
       framebuffer_object_support = true;
    }

    4. When using the function:

    if (framebuffer_object_support) {
       pglGenRenderbuffers(1, &color_rb)
       // etc.
    }
   From:   Frederic Bouvier <fredfgfs01@free.fr>
   Reply-To:   FlightGear developers discussions <flightgear-devel@lists.sourceforge.net>
   To:     FlightGear developers discussions <flightgear-devel@lists.sourceforge.net>
   Subject:    Re: [Flightgear-devel] OpenGL EXT Functions - native windows
   Date:   Mon, 3 May 2010 10:17:51 +0200 (CEST)

   The portable way is to use SGLookupFunction. SGGetGLProcAddress is only available under Unix

   -------------------------------------------------------------- */
#include "config.h"
#include <simgear/compiler.h>
#include <simgear/screen/extensions.hxx>

#include "sg_ext_funcs.hxx"

SG_Ext_Funcs::SG_Ext_Funcs()
{
    init_sg_ext_funcs();
}

SG_Ext_Funcs::~SG_Ext_Funcs()
{

}

void SG_Ext_Funcs::init_sg_ext_funcs(void)
{
    IsValid = false;
    // GLboolean glIsRenderbufferProc
    pglIsRenderbuffer = NULL;    // (GLuint renderbuffer)
    // void glBindRenderbufferProc
    pglBindRenderbuffer = NULL;    // (GLenum target, GLuint renderbuffer)
    // void glDeleteRenderbuffersProc
    pglDeleteRenderbuffers = NULL; // (GLsizei n, const GLuint* renderbuffers)
    // void glGenRenderbuffersProc 
    pglGenRenderbuffers = NULL;    // (GLsizei n, GLuint* renderbuffers)
    // void glRenderbufferStorageProc 
    pglRenderbufferStorage = NULL;  //(GLenum target, GLenum internalformat, GLsizei width, GLsizei height)
    // void glGetRenderbufferParameterivProc 
    pglGetRenderbufferParameteriv = NULL; // (GLenum target, GLenum pname, GLint* params)
    // GLboolean glIsFramebufferProc 
    pglIsFramebuffer = NULL;  // (GLuint framebuffer)
    // void glBindFramebufferProc 
    pglBindFramebuffer = NULL;  // (GLenum target, GLuint framebuffer)
    // void glDeleteFramebuffersProc 
    pglDeleteFramebuffers = NULL;   // (GLsizei n, const GLuint* framebuffers)
    // void glGenFramebuffersProc 
    pglGenFramebuffers = NULL; // (GLsizei n, GLuint* framebuffers)
    // GLenum glCheckFramebufferStatusProc 
    pglCheckFramebufferStatus = NULL; // (GLenum target)
    // void glFramebufferRenderbufferProc 
    pglFramebufferRenderbuffer = NULL; // (GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)
    // void glFramebufferTexture2DProc 
    pglFramebufferTexture2D = NULL; // (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
    // void glGetFramebufferAttachmentParameterivProc 
    pglGetFramebufferAttachmentParameteriv = NULL; // (GLenum target, GLenum attachment, GLenum pname, GLint* params)
    // void glGenerateMipmapProc 
    pglGenerateMipmap = NULL;   // (GLenum target)

    if (SGIsOpenGLExtensionSupported("GL_EXT_framebuffer_object"))
    {
        // Used by Map
        //  glGenFramebuffersEXT(1, &fbo);
        pglGenFramebuffers = (glGenFramebuffersProc)SGLookupFunction("glGenFramebuffersEXT"); // (GLsizei n, GLuint* framebuffers)
        //  glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo);
        pglBindFramebuffer = (glBindFramebufferProc)SGLookupFunction("glBindFramebufferEXT");  // (GLenum target, GLuint framebuffer)
        //  glGenRenderbuffersEXT(1, &rbo);
        pglGenRenderbuffers = (glGenRenderbuffersProc)SGLookupFunction("glGenRenderbuffersEXT");    // (GLsizei n, GLuint* renderbuffers)
        //  glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, rbo);
        pglBindRenderbuffer = (glBindRenderbufferProc)SGLookupFunction("glBindRenderbufferEXT");    // (GLenum target, GLuint renderbuffer)
        //  glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_RGB, textureSize, textureSize);
        pglRenderbufferStorage = (glRenderbufferStorageProc)SGLookupFunction("glRenderbufferStorageEXT");  //(GLenum target, GLenum internalformat, GLsizei width, GLsizei height)
        //  glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, rbo);
        pglFramebufferRenderbuffer = (glFramebufferRenderbufferProc)SGLookupFunction("glFramebufferRenderbufferEXT"); // (GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer) (Map)
        //  glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) == GL_FRAMEBUFFER_COMPLETE_EXT);
        pglCheckFramebufferStatus = (glCheckFramebufferStatusProc)SGLookupFunction("glCheckFramebufferStatusEXT"); // (GLenum target)
        //  glDeleteRenderbuffersEXT(1, &rbo);
        pglDeleteRenderbuffers = (glDeleteRenderbuffersProc)SGLookupFunction("glDeleteRenderbuffersEXT"); // (GLsizei n, const GLuint* renderbuffers)
        //  glDeleteFramebuffersEXT(1, &fbo);
        pglDeleteFramebuffers = (glDeleteFramebuffersProc)SGLookupFunction("glDeleteFramebuffersEXT");   // (GLsizei n, const GLuint* framebuffers)

        if ( pglGenFramebuffers && pglBindFramebuffer &&
             pglGenRenderbuffers && pglBindRenderbuffer &&
             pglRenderbufferStorage && pglFramebufferRenderbuffer &&
             pglCheckFramebufferStatus &&
             pglDeleteRenderbuffers && pglDeleteFramebuffers )
        {
            IsValid = true;
        }
    }
}

// eof - sg_ext_funcs.cxx





