#pragma once
// Diagnostic builds only. No hooks or synchronization are added to the library.
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <GL/glcorearb.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
extern "C" unsigned long glfwGetX11Window(GLFWwindow*);
namespace presentation {
inline unsigned long sequence{}, frame{}, snapshot{};
inline bool trace() { return std::getenv("MYGUI_PRESENT_TRACE") != nullptr; }
inline void log(const char* tag, long value=0) noexcept {
    if (!trace()) return;
    const auto us=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    std::fprintf(stderr,"%lu %lld %s %ld frame=%lu context=%p\n",++sequence,static_cast<long long>(us),tag,value,frame,static_cast<void*>(glfwGetCurrentContext()));
    std::fflush(stderr);
}
template<class T> T proc(const char* name) { return reinterpret_cast<T>(glfwGetProcAddress(name)); }
inline void environment() {
    const auto get=proc<PFNGLGETSTRINGPROC>("glGetString");
    std::fprintf(stderr,"ENV native_client=%lu glfw=%s platform=%d GL_vendor=%s GL_renderer=%s GL_version=%s\n",glfwGetX11Window(glfwGetCurrentContext()),glfwGetVersionString(),
#if GLFW_VERSION_MAJOR > 3 || (GLFW_VERSION_MAJOR == 3 && GLFW_VERSION_MINOR >= 4)
        glfwGetPlatform(),
#else
        0,
#endif
        get(GL_VENDOR),get(GL_RENDERER),get(GL_VERSION));
}
inline void capture(const char* phase, GLenum buffer) {
    const char* directory=std::getenv("MYGUI_PRESENT_CAPTURE_DIR");
    if (!directory) return;
    if (const char* selection=std::getenv("MYGUI_PRESENT_READ_PHASE")) {
        const std::string chosen=selection;
        if (chosen!="all" && chosen!=phase) return;
    }
    const auto get=proc<PFNGLGETINTEGERVPROC>("glGetIntegerv");
    const auto select=proc<PFNGLREADBUFFERPROC>("glReadBuffer");
    const auto read=proc<PFNGLREADPIXELSPROC>("glReadPixels");
    const auto error=proc<PFNGLGETERRORPROC>("glGetError");
    GLint fbo{},original{},pack{}; get(GL_READ_FRAMEBUFFER_BINDING,&fbo); get(GL_READ_BUFFER,&original);get(GL_PACK_ALIGNMENT,&pack);
    int w{},h{}; glfwGetFramebufferSize(glfwGetCurrentContext(),&w,&h);
    const auto currentGLX=proc<void*(*)()>("glXGetCurrentContext");
    const auto drawableGLX=proc<unsigned long(*)()>("glXGetCurrentDrawable");
    std::fprintf(stderr,"GLX context=%p drawable=%lu\n",currentGLX ? currentGLX() : nullptr,drawableGLX ? drawableGLX() : 0);
    log("gl-read-begin",buffer);
    std::fprintf(stderr,"READ phase=%s frame=%lu context=%p read_fbo=%d original_read_buffer=%d selected=%u dimensions=%dx%d pack_alignment=%d\n",phase,frame,static_cast<void*>(glfwGetCurrentContext()),fbo,original,buffer,w,h,pack);
    if (w<=0 || h<=0 || fbo!=0) { log("gl-read-skipped"); return; }
    const auto pixelStore=proc<PFNGLPIXELSTOREIPROC>("glPixelStorei");
    pixelStore(GL_PACK_ALIGNMENT,1); select(buffer);
    std::vector<unsigned char> pixels(static_cast<std::size_t>(w)*h*3);
    read(0,0,w,h,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
    const auto glError=error(); select(static_cast<GLenum>(original)); pixelStore(GL_PACK_ALIGNMENT,pack);
    const auto filename=std::to_string(++snapshot)+"-f"+std::to_string(frame)+"-"+phase+".ppm";
    std::filesystem::create_directories(directory);
    std::ofstream stream(std::filesystem::path{directory}/filename,std::ios::binary);
    stream<<"P6\n"<<w<<" "<<h<<"\n255\n";
    for (int y=h-1;y>=0;--y) stream.write(reinterpret_cast<const char*>(pixels.data()+static_cast<std::size_t>(y)*w*3),w*3);
    std::fprintf(stderr,"READ file=%s error=%u\n",filename.c_str(),glError); log("gl-read-end");
}
inline bool command(int key) {
    if (key!=GLFW_KEY_F12) return false;
    capture("idle-after-last-swap-front",GL_FRONT);
    capture("idle-after-last-swap-back-undefined",GL_BACK);
    log("capture-command-done"); return true;
}
} // namespace presentation
