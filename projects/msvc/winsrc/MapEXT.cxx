// MapEXT.cxx

#include "config.h"
#include <simgear/compiler.h>
#include <simgear/screen/extensions.hxx>

#include "sg_ext_funcs.hxx"
#include "MapEXT.hxx"

/* -----------------------------------------
   Map options
    } else if (strcmp(arg, "--render-offscreen") == 0) {
	renderToFramebuffer = true;
    } else if (strcmp(arg, "--render-to-window") == 0) {
	renderToFramebuffer = false;
   ----------------------------------------- */

SG_Ext_Funcs * sg_ext_funcs = NULL;

bool Map_Init_Ext(GLuint * pfbo, GLuint * prbo, GLint textureSize)
{
    bool result = false;
    sg_ext_funcs = new SG_Ext_Funcs;
    if (sg_ext_funcs->IsValid) {
    	// Try to get a framebuffer.  First, check if the requested
	    // size is supported.
    	GLint max;
	    glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE_EXT, &max);
	    if (textureSize > max) {
            fprintf(stderr, "ERROR: Requested buffer size (%d) > maximum supported buffer size (%d)\n", textureSize, max);
            return result;
        }
        sg_ext_funcs->pglGenFramebuffers(1, pfbo); // glGenFramebuffersEXT(1, &fbo);
        sg_ext_funcs->pglBindFramebuffer(GL_FRAMEBUFFER_EXT, *pfbo); // glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo);

        sg_ext_funcs->pglGenRenderbuffers(1, prbo); // glGenRenderbuffersEXT(1, &rbo);
        sg_ext_funcs->pglBindRenderbuffer(GL_RENDERBUFFER_EXT, *prbo); // glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, rbo);
        sg_ext_funcs->pglRenderbufferStorage(GL_RENDERBUFFER_EXT, 
			     GL_RGB, textureSize, textureSize); // glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_RGB, textureSize, textureSize);
        sg_ext_funcs->pglFramebufferRenderbuffer(GL_FRAMEBUFFER_EXT,
				 GL_COLOR_ATTACHMENT0_EXT,
				 GL_RENDERBUFFER_EXT,
				 *prbo); // glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,GL_COLOR_ATTACHMENT0_EXT,GL_RENDERBUFFER_EXT,rbo);
        result = (sg_ext_funcs->pglCheckFramebufferStatus(GL_FRAMEBUFFER_EXT) == GL_FRAMEBUFFER_COMPLETE_EXT);
        // glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) == GL_FRAMEBUFFER_COMPLETE_EXT);
        if (!result) {
            sg_ext_funcs->IsValid = false;
        }
    }
    return result;
}

void Map_Exit_Ext(GLuint * pfbo, GLuint * prbo)
{
    if (sg_ext_funcs->IsValid)
    {
        if (*prbo != 0)
        {
            sg_ext_funcs->pglDeleteRenderbuffers(1, prbo); //  glDeleteRenderbuffersEXT(1, &rbo);
            *prbo = 0;
        }
        if (*pfbo != 0)
        {
            sg_ext_funcs->pglDeleteFramebuffers(1,pfbo); // glDeleteFramebuffersEXT(1, &fbo);
            *pfbo = 0;
        }
    }
}

// eof - MapEXT.cxx
