#!/usr/bin/env python3
"""Build opt-in traced copies of the current ComboBox example and GLFW control.
Production sources stay unchanged; generated sources and evidence live in build/.
"""
import pathlib
import shutil
import shlex
import subprocess

repo = pathlib.Path(__file__).resolve().parents[2]
out = repo / 'build/presentation-diagnostics/probe'
out.mkdir(parents=True, exist_ok=True)
shutil.copytree(repo / 'include/gui', out / 'include/gui', dirs_exist_ok=True)
shutil.copytree(repo / 'assets', out / 'assets', dirs_exist_ok=True)
shutil.copyfile(repo / 'tests/diagnostics/PresentationTrace.hpp', out / 'PresentationTrace.hpp')
header = (out / 'include/gui/UIContext.hpp').read_text()
header = header.replace('#pragma once', '#pragma once\n#include "PresentationTrace.hpp"', 1)
header = header.replace('void invalidateLayout() noexcept {', 'void invalidateLayout() noexcept { presentation::log("invalidate-layout");')
header = header.replace('void requestRedraw() noexcept {', 'void requestRedraw() noexcept { presentation::log("request-redraw");')
(out / 'include/gui/UIContext.hpp').write_text(header)
source = (repo / 'src/UIContext.cpp').read_text()
source = source.replace('redraw_ = true;', 'do { presentation::log("redraw-set"); redraw_ = true; } while(false);')
source = source.replace('redraw_ = false;', 'do { presentation::log("redraw-clear"); redraw_ = false; } while(false);')
(out / 'UIContext.cpp').write_text(source)
source = (repo / 'src/Window.cpp').read_text()
source = '#include "PresentationTrace.hpp"\n' + source
source = source.replace('    glfwSwapBuffers(state_->window_);', '    presentation::log("swap-enter"); glfwSwapBuffers(state_->window_); presentation::log("swap-return");')
source = source.replace('    processEvents(timeoutSeconds);', '    presentation::log("wait-enter"); processEvents(timeoutSeconds); presentation::log("wait-return");')
(out / 'Window.cpp').write_text(source)
source = (repo / 'examples/common/Example.hpp').read_text()
source = source.replace('#pragma once', '#pragma once\n#include "PresentationTrace.hpp"', 1)
source = source.replace('    window.setEventDelivery', '    presentation::environment();\n    window.setEventDelivery', 1)
source = source.replace('        ui.layout(window.viewport());', '        presentation::log("layout-enter",ui.needsRedraw()); ui.layout(window.viewport()); presentation::log("layout-return",ui.needsRedraw());')
source = source.replace('            window.makeContextCurrent();', '            ++presentation::frame; presentation::log("frame-start",ui.needsRedraw()); window.makeContextCurrent();')
source = source.replace('            renderer.endFrame();', '            renderer.drawRect({8,static_cast<float>(window.viewport().logicalHeight-20),8,8},{1,0,1,1});\n            renderer.endFrame(); presentation::log("frame-end",ui.needsRedraw());\n            if (!maxFrames || ui.needsRedraw() || presentation::frame==1) presentation::capture("before-swap-back",GL_BACK);')
source = source.replace('            window.swapBuffers();', '            window.swapBuffers();\n            if (!maxFrames || presentation::frame==1) presentation::capture("after-swap-front",GL_FRONT);')
source = source.replace('        while (auto event = window.nextEvent()) {', '''        while (auto event = window.nextEvent()) {
            presentation::log("event",static_cast<long>(event->type()));
            if (const auto* key=event->getIf<gui::KeyEvent>(); key && key->key==gui::Key::F12) {
                if(key->action==gui::KeyAction::Press) presentation::command(GLFW_KEY_F12);
                continue;
            }
            presentation::log("dispatch-enter",ui.needsRedraw());''')
source = source.replace('ui.handleEvent(*event) && onEvent) onEvent(*event);', 'ui.handleEvent(*event) && onEvent) onEvent(*event);\n            presentation::log("dispatch-return",ui.needsRedraw());')
(out / 'Example.hpp').write_text(source)
flags = ['c++', '-std=c++20', '-Wall', '-Wextra', '-Wpedantic', '-g', '-I'+str(out), '-I'+str(out/'include'), '-I'+str(repo/'third_party/khronos')]
libs = shlex.split(subprocess.check_output(['pkg-config', '--libs', 'glfw3', 'freetype2'], text=True))
subprocess.run(flags + [str(repo/'examples/combobox.cpp'), str(out/'UIContext.cpp'), str(out/'Window.cpp'), str(repo/'build/debug/libMyGUI_core.a'), *libs, '-o', str(out/'combobox')], check=True)
subprocess.run(flags + [str(repo/'tests/diagnostics/glfw_presentation.cpp'), *libs, '-o', str(out/'control')], check=True)
print(out)
