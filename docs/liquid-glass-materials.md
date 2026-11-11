# Chat chrome materials

## Decision

Keep the existing Qt **6.9.3** dependency and Moltorino's existing frame,
layout, widgets and input handling. Use documented Windows DWM system backdrops
where supported, with a small cached Qt optical layer for chrome. No new
third-party runtime library, downloaded DLL, shader compiler or deployment
dependency is introduced.

Mica is a Windows material, not Apple's Liquid Glass. The additional optical
layer provides diffusion, bounded refraction at rounded edges, directional
reflection, a soft rim and stable fine grain. It is an approximation of a glass
slab, not Apple's renderer. Chat text and all foreground controls remain sharp.
Glass belongs to navigation, headers, the composer and popovers; individual
messages retain the existing flat, opaque reading surface.

## Approaches evaluated

| Approach | Rendering and compatibility | Decision |
| --- | --- | --- |
| [DWM system backdrops](https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/ne-dwmapi-dwm_systembackdrop_type) | Documented Windows 11 build 22621+ API. Main-window Mica and transient-window Acrylic are composed by Windows. Neither performs Apple-style lensing. | Use on eligible normally framed/custom-caption HWNDs. Preserve native resize, snap and hit testing. |
| [Mica](https://learn.microsoft.com/en-us/windows/apps/design/style/mica) | Wallpaper-based material; Windows samples wallpaper rather than continuously blurring other applications. | Useful stable foundation, combined with the app-owned optical layer. |
| [QWindowKit](https://github.com/stdware/qwindowkit/tree/1.5.0) 1.5.0, Apache-2.0 | Reviewed release metadata, license, build integration and Windows implementation. Windows materials wrap DWM; broader frame/native-event integration would replace working infrastructure. Current macOS glass support does not supply the same renderer on Windows. | Do not add a frame framework to obtain DWM calls already available directly. No dependency installed. |
| [FramelessHelper](https://github.com/wangwenx190/framelesshelper), MIT | Archived predecessor, directs new development to QWindowKit. Provides frame integration and platform materials, not a Windows Apple-like refraction renderer. | Do not add an archived frame dependency. No dependency installed. |
| [Qt Acrylic Material](https://github.com/Qt-Widgets/qtacrylicmaterial), MIT | Reviewed advertised architecture/license and documented GPU/power warning. Older Qt Widgets/Quick approach would add rendering integration beyond the small chrome surfaces. | No dependency installed. |
| [QRhiWidget](https://doc.qt.io/archives/qt-6.9/qrhiwidget.html), Qt 6.9.3 | Supports GPU rendering, including D3D11 on Windows. Requires window-wide graphics-backend consistency, shader resources and additional texture/composition integration with the existing raster widgets. RHI compatibility must be checked for every Qt minor upgrade. | Investigated, not adopted. A GPU refraction shader would still need an app-owned source texture; bounded cached raster work is sufficient for these small surfaces. |
| [Qt Shader Tools](https://doc.qt.io/archives/qt-6.9/qtshadertools-index.html), Qt 6.9.3 | Offline `qsb` can package portable shaders; build tool and runtime components have different Qt licenses. | Not needed by the chosen renderer; no module or runtime added. |

These are source/documentation evaluations, not claims of having integrated and
benchmarked every candidate. The implemented renderer is exercised by the
standalone native Qt material check and the actual packaged application.
[Apple's design discussion](https://developer.apple.com/videos/play/wwdc2025/219/)
informs restrained layering and edge behavior, not a claim of native Apple API use.

## Rendering and performance

`GlassMaterial` shares one sampler per visible chat view. It samples only the
top/bottom 40 logical pixels, reduces them to at most 384 × 32 pixels while
preserving aspect ratio, and approximates Gaussian diffusion with three separable
box passes. Dirty visible views are sampled at most four times per second.
Hidden/minimized views stop sampling. Only chrome regions are invalidated after
a changed sample; this does not add another full-chat repaint loop.

The optical layer derives rounded-slab normals from a signed distance function.
Near the rim, these normals displace bilinear samples of the actual diffused
image by at most approximately 1.4 logical pixels, using an index of refraction
of 1.46. Directional edge reflection and low-amplitude stable grain complete the
surface. The center is not displaced. Layer alpha is 98% in the fallback and
68% over native DWM; neutral tint keeps transmission subdued.

Size, DPI, options and source generation key the cached pixmap. Stable chrome
reuses that pixmap. The material raster is limited to 2048 pixels in width at
high DPI. Menus/popovers take a single clipped snapshot at opening or movement;
outside the owning chat view the source is neutral. They do not subscribe to
continuous scrolling updates. Button feedback uses the existing short Qt
animations; no animation object is created per message.

The standalone check verifies nonzero edge displacement against a refraction-off
reference, unchanged center pixels, rounded alpha masking, one material rebuild
for 100 unchanged paints, and the capture budget under repeated chat painting.
A local optimized Qt 6.9.3 Linux reference measured approximately 2.6 ms per
1000 × 36 rebuild and four samples during 133 paints in 1.25 seconds. This is a
development-machine measurement, not a universal Windows frame-time guarantee.
Windows CI records its own measurements and native widget captures.

## Security, licensing and fallback

- The implementation uses only the existing Qt dependency and Windows system
  Dwmapi/Advapi32. Windows DLLs are supplied by the OS, never downloaded or
  injected. Added code is MIT; existing Qt and bundled dependency licenses and
  deployment remain unchanged.
- No new networking, telemetry, process inspection, authentication hooks,
  account/token access, administrator requirement or security-setting changes.
  Registry access only reads the user's existing transparency preference.
- Backdrop sources are explicitly registered chat views. No desktop/screen
  capture, login widgets, account fields or unrelated application windows are
  sampled. Temporary images remain in process memory and are cleared on hide.
  Only the explicit CI review mode writes synthetic chat screenshots.
- Native materials require build 22621+, a compatible HWND and a successful DWM
  call. Qt-frameless/layered popups keep the Qt fallback; changing their opacity
  would interfere with native composition. Unsupported systems use the cached
  app-owned material without an OS backdrop. Chat itself stays opaque.
- High contrast or the Windows transparency switch disables sampling/refraction
  and forces opaque surfaces. Relevant Windows setting/theme/composition events
  refresh this policy. [Windows material fallbacks](https://learn.microsoft.com/en-us/windows/apps/develop/ui/materials)
  can additionally depend on power/GPU policy.

Compilation and packaging remain on Windows Server 2022 / Visual Studio 2022.
An additional Windows Server 2025 job launches the **same extracted ZIP**, records
DWM support/result/active status and captures the UI. A server CI capture is not
a substitute for checking wallpaper transmission on an interactive Windows 11
desktop; native appearance still depends on OS composition policy.

## Composer and package validation

The composer retains its automatic multiline growth, replies, completion,
translation, channel tools and send behavior. Status metadata shares the action
row instead of forcing an extra empty row. Actions use 24-pixel logical hit areas
with 16-pixel glyphs. Margins are scaled; user font and application scale settings
are preserved.

The existing-profile UI fixtures assert a single-line height of at most 44
logical pixels, multiline growth, compact visible SVG controls and unchanged
settings files. Dark, Glass, Moltorino Midnight and Light are exercised.
The package still identifies itself as `Moltorino M15.5.2`, non-Nightly, normal
`%APPDATA%\Chatterino2\Settings` mode, without a portable marker. Extracted-package
tests retain isolated runtime/DLL checks and FFmpeg/FFprobe launches. The material
preview and native crash diagnostic programs are CI helpers, not package contents.
