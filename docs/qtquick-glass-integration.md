# GPU glass integration, conservative first milestone

Branch `liquid-glass-integration-01` keeps the entire real Twitch and IRC
message rendering stack unchanged. It introduces a reusable
`tools/glass-lab/qml/LiquidGlassPanel.qml` with:
`backgroundLayer`, `active`, `refraction`, `vibrancy`, `edgeLight`,
`dispersion`, `motionX`, and `motionY`. The QSB shader is shared with
the standalone lab rather than duplicated in the actual app.

Build flag `MOLTORINO_GLASS_QUICK_PREVIEW` defaults OFF. When explicitly
ON, Settings -> Interface -> Open GPU Liquid Glass preview opens a modal
QtWidgets-hosted QQuickView in its own dialog. The preview's backdrop and
messages are simulated. No real chat, OAuth, browser, Windows desktop
or other windows are captured.

## Resizing regression addressed

Build 05 reallocated the ShaderEffectSource texture buffer at every
intermediate window size, while the large absolute-positioned test scene
was reflowing. Rapid changes could cause texture misregistration or cropped
shapes around the composer (exact hardware-dependent root cause still
requires repro and testing).

The lab and preview now keep a stable design-sized 1080x720 / 850x480
stage and apply a uniform QML scale transform when resized. This keeps
composer bounds, canvas-to-shader UV mapping, and backing texture
dimensions stable. Let ShaderEffectSource choose the native device-pixel
texture size instead of specifying logical-pixel dimensions.

CI creates a resized lab screenshot at 760x507 in addition to ON/OFF
captures. This is a static resize test; interactive resizing and FPS still
require user validation on their GPU.

## Not yet implemented

No production replacement of Split/ChannelView/real composer, no full
QAbstractListModel or emote delegate, no external-desktop refraction, and
no measured FPS. The existing Qt painter plugin stays separate.
