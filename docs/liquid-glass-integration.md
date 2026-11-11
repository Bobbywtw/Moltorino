# Integrated Liquid Glass (validation in progress)

This work uses `moltorino-build-optimization-01` and its measured Windows cache,
compiler and deployment configuration. The first integration commit is `a7ec73d`.
Windows execution and visual acceptance are pending; a successful component test
is not acceptance of the application's appearance.

## Renderer and native controls

`src/widgets/glass/GlassChrome.cpp` creates one Qt Quick Widgets underlay per
application window. `GlassChrome.qml` places the existing `LiquidGlassPanel.qml`
material at the actual Qt widget coordinates. Native widgets continue to own
labels, icons, badges, emotes, text editing, accessibility, focus, actions and
hit testing. The underlay has no focus and accepts no mouse input.

The production executable links the same `glass.vert` and `glass.frag` as the
approved Build #5 (`2705509`). Both shader source files are unchanged. Production
uses that prototype's 1.35 refraction, 1.45 vibrancy, 2.35 edge lighting and 0.65 px
dispersion. The only shared-panel API addition supplies an explicit backdrop
origin, so widget motion and resizing invalidate texture coordinates immediately.
No appearance-module DLL or separate preview window supplies the application UI.

Actual tabs, channel headers, composer, title bar and common controls register
geometry with this scene. Secondary windows receive the material too. Ordinary
Qt control styles only expose the underlay; native control foregrounds remain
native. Menus replay their original Qt foreground painting above the underlay,
while keeping the original actions, shortcuts, selection and submenu handling.
Autocomplete and account-switcher list fills expose the material. Emote-popup
views use their existing transparent-background API; the scrolling chat view
keeps its original opaque reading surface.

The backdrop extends each real split's existing wallpaper crop and respects the
continuous-background setting. Static images are cached. Existing video decode
is shared; chrome updates are limited to eight per second. Popovers sample only
the owning Moltorino window, in memory, with bounded refresh. Desktop pixels and
account storage are never captured. Geometry follows Qt logical coordinates and
native device pixel ratios, rather than stretching fixed-size panel textures.

Hover/selection transitions take 140–150 ms, press feedback 70 ms with a 0.7 px
material offset, and chrome opening fades 120 ms. Animation objects belong to
persistent controls. Normal messages and scrolling receive no new animation or
per-message effect.

Qt Quick software rendering and scene-graph errors restore readable native
controls. Hidden windows release scene-graph resources. The pre-existing native
Windows frame, resize, snap and hit-test code remains in charge.

## Dependencies and packaging

Qt Quick Widgets is part of the already pinned official Qt **6.9.3** SDK. Its
public integration API avoids an additional frameless-window library, private
Qt rendering headers, injection, admin privileges or a network-enabled plugin.
The material contains no telemetry or external URLs. QML, shaders and components
are compiled into the executable. Qt runtime DLLs are deployed by `windeployqt`;
the package explicitly checks for `Qt6QuickWidgets.dll`, `Qt6Quick.dll` and
`Qt6Qml.dll`. Qt LGPL/GPL license texts and the exact upstream source locations
accompany the Windows ZIP.

The VS2022, Conan, JOM, libwebm, YASM/libvpx, FFmpeg/pkg-config, VC runtime and
media-executable fixes are retained. Production LTO remains enabled, Nightly
remains disabled and the actual project version remains **15.5.2**. The package
uses `%APPDATA%\Chatterino2\Settings`; no `modes` or `portable` marker is added.
The old `glassSkinEnabled=false` setting does not suppress the new renderer.
No settings migration or account-data rewrite is performed.

## Required executable review

`.CI/verify-glass-ui.ps1` launches the packaged `Moltorino7.exe`, with a minimal
system PATH, through the real normal settings directory. It refuses to overwrite
an existing runner profile and verifies that fixture files remain byte-identical.
Cases include the old appearance switch disabled and an existing bundled
wallpaper. Captures come from the application itself, not the lab:

- Main chat, narrow resize, hover and pressed states.
- The real channel context menu, settings, emote popup and native Qt dialog.
- Material ON/OFF pixels in the same executable and layout.
- Per-window graphics API, QML readiness, visible surface bounds and baked shader
  hashes, plus composer text and multiline/resize measurements.

Windows must use D3D11 and the real tab/header/composer/control material surfaces.
The ZIP must launch the application, FFmpeg and FFprobe with packaged runtimes.
The resulting screenshots must still be inspected against the old executable
and Build #5 before declaring visual completion.

Changes to production QML/shaders require the full workflow. Isolated lab-only
changes keep the fast path. The compatible-executable fingerprint now includes
production shader resources, preventing stale native packages from validating a
changed production material.

## Scope limits to verify and communicate

A uniform backdrop has little detail to refract; rim lighting alone does not
prove optical integration. Wallpaper and real popover-content captures must show
actual displacement. Popovers sample only the owning application, so areas
outside that window use the application background. OS-owned file pickers and
other operating-system UI retain their native rendering. Native message content,
checkbox/radio glyphs and data-view foregrounds remain native. Video/popover
backdrops have a bounded update rate; interaction lighting runs independently.
This is a Qt shader material, not a claim to reproduce Apple's proprietary
renderer exactly.

References: [Qt Quick Widgets](https://doc.qt.io/qt-6/qquickwidget.html),
[ShaderEffect](https://doc.qt.io/qt-6/qml-qtquick-shadereffect.html), and
[ShaderEffectSource](https://doc.qt.io/qt-6/qml-qtquick-shadereffectsource.html).
