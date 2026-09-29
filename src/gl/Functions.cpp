#include "Functions.hpp"

#include <stdexcept>
#include <string>

namespace gui::gl {

#define MYGUI_GL_DEFINE(type, name) type name{nullptr};
MYGUI_GL_FUNCTIONS(MYGUI_GL_DEFINE)
#undef MYGUI_GL_DEFINE

void load(GLProcLoader loader) {
    if (!loader) {
        throw std::runtime_error("No OpenGL function loader provided");
    }

    // GLProc is void(*)(), which converts to any function pointer type.
#define MYGUI_GL_LOAD(type, name) \
    name = reinterpret_cast<type>(loader("gl" #name)); \
    if (!name) { \
        throw std::runtime_error(std::string{"Missing OpenGL function: gl"} + #name); \
    }

    MYGUI_GL_FUNCTIONS(MYGUI_GL_LOAD)
#undef MYGUI_GL_LOAD
}

} // namespace gui::gl
