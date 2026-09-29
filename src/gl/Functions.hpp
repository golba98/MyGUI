#pragma once

// Internal OpenGL entry points, loaded at runtime. Only renderer sources
// include this; the public headers never expose OpenGL types.

#include "gui/Renderer.hpp"

#include <GL/glcorearb.h>

// X(type, name): every function the renderer uses, bound as gui::gl::name.
#define MYGUI_GL_FUNCTIONS(X) \
    X(PFNGLVIEWPORTPROC, Viewport) \
    X(PFNGLENABLEPROC, Enable) \
    X(PFNGLDISABLEPROC, Disable) \
    X(PFNGLBLENDFUNCSEPARATEPROC, BlendFuncSeparate) \
    X(PFNGLDRAWARRAYSPROC, DrawArrays) \
    X(PFNGLCREATESHADERPROC, CreateShader) \
    X(PFNGLSHADERSOURCEPROC, ShaderSource) \
    X(PFNGLCOMPILESHADERPROC, CompileShader) \
    X(PFNGLGETSHADERIVPROC, GetShaderiv) \
    X(PFNGLGETSHADERINFOLOGPROC, GetShaderInfoLog) \
    X(PFNGLDELETESHADERPROC, DeleteShader) \
    X(PFNGLCREATEPROGRAMPROC, CreateProgram) \
    X(PFNGLATTACHSHADERPROC, AttachShader) \
    X(PFNGLDETACHSHADERPROC, DetachShader) \
    X(PFNGLLINKPROGRAMPROC, LinkProgram) \
    X(PFNGLGETPROGRAMIVPROC, GetProgramiv) \
    X(PFNGLGETPROGRAMINFOLOGPROC, GetProgramInfoLog) \
    X(PFNGLDELETEPROGRAMPROC, DeleteProgram) \
    X(PFNGLUSEPROGRAMPROC, UseProgram) \
    X(PFNGLGETUNIFORMLOCATIONPROC, GetUniformLocation) \
    X(PFNGLUNIFORM2FPROC, Uniform2f) \
    X(PFNGLGENVERTEXARRAYSPROC, GenVertexArrays) \
    X(PFNGLBINDVERTEXARRAYPROC, BindVertexArray) \
    X(PFNGLDELETEVERTEXARRAYSPROC, DeleteVertexArrays) \
    X(PFNGLGENBUFFERSPROC, GenBuffers) \
    X(PFNGLBINDBUFFERPROC, BindBuffer) \
    X(PFNGLBUFFERDATAPROC, BufferData) \
    X(PFNGLDELETEBUFFERSPROC, DeleteBuffers) \
    X(PFNGLENABLEVERTEXATTRIBARRAYPROC, EnableVertexAttribArray) \
    X(PFNGLVERTEXATTRIBPOINTERPROC, VertexAttribPointer)

namespace gui::gl {

// Process-wide function bindings, like those of any GL loader; they hold no
// renderer state.
#define MYGUI_GL_DECLARE(type, name) extern type name;
MYGUI_GL_FUNCTIONS(MYGUI_GL_DECLARE)
#undef MYGUI_GL_DECLARE

// Resolves every function above through loader. Requires a current context.
// Safe to call more than once. Throws std::runtime_error if any is missing.
void load(GLProcLoader loader);

} // namespace gui::gl
