// sg_ext_funcs.hxx

// Use SG extended GL functions

class SG_Ext_Funcs
{
public:
    SG_Ext_Funcs();
    ~SG_Ext_Funcs();

    void init_sg_ext_funcs(void);

    // GLboolean
    glIsRenderbufferProc pglIsRenderbuffer;    // (GLuint renderbuffer)
    // void 
    glBindRenderbufferProc pglBindRenderbuffer;    // (GLenum target, GLuint renderbuffer) (Map)
    // void
    glDeleteRenderbuffersProc pglDeleteRenderbuffers; // (GLsizei n, const GLuint* renderbuffers) (Map)
    // void
    glGenRenderbuffersProc pglGenRenderbuffers;    // (GLsizei n, GLuint* renderbuffers) (Map)
    // void
    glRenderbufferStorageProc pglRenderbufferStorage;  //(GLenum target, GLenum internalformat, GLsizei width, GLsizei height) (Map)
    // void
    glGetRenderbufferParameterivProc pglGetRenderbufferParameteriv; // (GLenum target, GLenum pname, GLint* params)
    // GLboolean
    glIsFramebufferProc pglIsFramebuffer;  // (GLuint framebuffer)
    // void
    glBindFramebufferProc pglBindFramebuffer;  // (GLenum target, GLuint framebuffer) (Map)
    // void
    glDeleteFramebuffersProc pglDeleteFramebuffers;   // (GLsizei n, const GLuint* framebuffers) (Map)
    // void
    glGenFramebuffersProc pglGenFramebuffers;   // (GLsizei n, GLuint* framebuffers) (Map)
    // GLenum
    glCheckFramebufferStatusProc pglCheckFramebufferStatus; // (GLenum target) (Map)
    // void
    glFramebufferRenderbufferProc pglFramebufferRenderbuffer; // (GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer) (Map)
    // void
    glFramebufferTexture2DProc pglFramebufferTexture2D; // (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
    // void
    glGetFramebufferAttachmentParameterivProc pglGetFramebufferAttachmentParameteriv; // (GLenum target, GLenum attachment, GLenum pname, GLint* params)
    // void
    glGenerateMipmapProc pglGenerateMipmap;   // (GLenum target)

    bool IsValid;
};

// eof - sg_ext_funcs.hxx
