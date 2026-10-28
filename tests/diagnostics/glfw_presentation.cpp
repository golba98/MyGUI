// Minimal GLFW 3.3-core presentation control, with the same diagnostic readback.
#include "PresentationTrace.hpp"
#include <cstring>
namespace { bool dirty=true; }
int main(int argc,char** argv) {
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
    auto* window=glfwCreateWindow(640,520,"MyGUI presentation control",nullptr,nullptr);
    if (!window) {glfwTerminate();return 1;}
    glfwMakeContextCurrent(window); glfwSwapInterval(1); presentation::environment();
    glfwSetWindowRefreshCallback(window,[](GLFWwindow*){presentation::log("control-refresh");dirty=true;});
    glfwSetFramebufferSizeCallback(window,[](GLFWwindow*,int,int){presentation::log("control-resize");dirty=true;});
    glfwSetKeyCallback(window,[](GLFWwindow*,int key,int,int action,int){
        if(action==GLFW_PRESS) { if(!presentation::command(key)) dirty=true; }
    });
    const bool continuous=argc>1 && std::strcmp(argv[1],"--continuous")==0;
    const auto clearColor=presentation::proc<PFNGLCLEARCOLORPROC>("glClearColor");
    const auto clear=presentation::proc<PFNGLCLEARPROC>("glClear");
    const auto viewport=presentation::proc<PFNGLVIEWPORTPROC>("glViewport");
    const auto enable=presentation::proc<PFNGLENABLEPROC>("glEnable");
    const auto disable=presentation::proc<PFNGLDISABLEPROC>("glDisable");
    const auto scissor=presentation::proc<PFNGLSCISSORPROC>("glScissor");
    while(!glfwWindowShouldClose(window)) {
        if(dirty || continuous) {
            ++presentation::frame; presentation::log("frame-start");
            int w{},h{};glfwGetFramebufferSize(window,&w,&h);viewport(0,0,w,h);
            disable(GL_SCISSOR_TEST);clearColor(.07f,.08f,.11f,1);clear(GL_COLOR_BUFFER_BIT);
            enable(GL_SCISSOR_TEST);scissor(40,100,300,200);clearColor(1,.5f,0,1);clear(GL_COLOR_BUFFER_BIT);
            scissor(8,12,8,8);clearColor(1,0,1,1);clear(GL_COLOR_BUFFER_BIT);disable(GL_SCISSOR_TEST);
            if(!continuous || presentation::frame==1) presentation::capture("before-swap-back",GL_BACK);
            presentation::log("frame-end"); presentation::log("swap-enter"); glfwSwapBuffers(window); presentation::log("swap-return");
            if(!continuous || presentation::frame==1) presentation::capture("after-swap-front",GL_FRONT);
            dirty=false;
        }
        if(continuous) glfwPollEvents();
        else {presentation::log("wait-enter");glfwWaitEvents();presentation::log("wait-return");}
    }
    glfwDestroyWindow(window);glfwTerminate();
}
