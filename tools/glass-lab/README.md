# Moltorino Shader Lab — proof before any integration

This intentionally standalone Qt Quick `.qsb` shader prototype uses **app-owned
background content**, NOT Windows desktop capture, and does not alter the real
Moltorino binary, chats, profiles, or settings.

## Why no DXGI Desktop Duplication
Microsoft's DXGI duplication API returns the *composed* desktop texture.
It does not expose the unobstructed windows/wallpaper beneath our own opaque
window. Capturing the composed image and rendering it back onto our window
would also risk recursive self-capture. There is no documented sub-1ms latency
guarantee. WM_MOVE is a position notification, not a synchronization primitive
for DXGI, and must not trigger synchronous GPU blocking on the UI thread.

References:
- https://learn.microsoft.com/en-us/windows-hardware/drivers/display/desktop-duplication-api
- https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_2/nf-dxgi1_2-idxgioutputduplication-acquirenextframe
- https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-move

## What the lab tests

- Qt Quick scene graph, Vulkan-style GLSL, build-time `qsb` compilation
- ShaderEffectSource capturing an isolated app-owned backdrop
- Real pixel texture displacement at panel edges (not a painted border)
- Five-tap approximation to diffusion, adjustable vibrancy up to 1.5
- Cursor-reactive specular highlight and movement-driven parallax
- Baseline-vs-shader screenshot capture from the real running application

It is not a 1:1 Apple compositor or macOS Liquid Glass clone; Apple's actual
material algorithms are proprietary. The `vibrancy` factor is an aesthetic
tuning parameter, not a published Apple standard.

## Local Windows build (Qt 6.9.3 with ShaderTools)

```powershell
cmake -S tools/glass-lab -B build/glass-lab -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/glass-lab --parallel 2
build/glass-lab/MoltorinoGlassLab.exe --baseline --capture baseline.png
build/glass-lab/MoltorinoGlassLab.exe --capture glass.png
```

The dedicated GitHub workflow builds **only this small lab**, not the full
Moltorino client. It publishes Windows screenshots separately. Do not introduce
the native QQuickView inside Moltorino splits until the captures are visibly
different and the shader is confirmed to render on a GPU backend.
