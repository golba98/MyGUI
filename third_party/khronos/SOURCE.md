# Khronos OpenGL headers

- `GL/glcorearb.h` from KhronosGroup/OpenGL-Registry commit `9cb90ca4902d588bef3c830fbb1da484893bd5fb` (`api/GL/glcorearb.h`).
- `KHR/khrplatform.h` from KhronosGroup/EGL-Registry commit `57b4876de0f33677ece92dd9de0ef105ce69139d` (`api/KHR/khrplatform.h`).

Unmodified, under the MIT-style licenses in their headers. Windows and macOS do not ship
`GL/glcorearb.h`, so the library and its OpenGL tests use these copies on every platform. They are
private build dependencies and are not installed.
