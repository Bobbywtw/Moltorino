# Moltorino Qt Quick glass lab — optical surface test

This is an isolated Qt 6.9 shader lab, not an actual Moltorino chat build.
The app uses `QQuickView` and compiled `.qsb` vertex and fragment shaders,
with actual in-app texture sampling from a backdrop **outside the glass
foreground's capture tree**. There is no desktop or other-window capture.

## Revision 5 — text isolation, restrained dispersion, cursor lighting

- Neutral graphite scene with a detailed 24px grid, distinct concentric circles,
  sharp typography and two inherited accent colors to reveal color vibrancy
- Rounded rectangle signed-distance meniscus with a 31px edge shell;
  refracted pixels bend along edges while the center stays crisp. Build #4
  reduces the maximum displacement to avoid foldover at rounded corners
- Nine-tap approximation for backdrop diffusion (not multi-pass Gaussian)
- Saturation adjustable from 1.0 to 1.65; default 1.45 is a visual choice,
  **not a known or verifiable Apple material parameter**
- Pointer-position-dependent specular falloff around every curved edge, plus a separate glossy ridge above composer
- Separate post-shader foreground layer for sharp chat-like text and a badge, proving background distortion never affects the content
- Microscopic red/blue channel displacement limited to the meniscus, with live dispersion control (default 0.65px); no extra color tint
- Clickable refraction, vibrancy, edge light and dispersion values for easy live tuning
- **LENS OFF** uses the same geometry and background but bypasses all
  refraction, blur, vibrancy and specular material calculations in the shader
- Drag motion drives a short easing parallax displacement (no desktop capture)

## How to inspect

Run `MoltorinoGlassLab.exe` from the dedicated build artifact. Compare ON
against OFF. Move the cursor along panel edges to see a localized highlight,
then vary DISPERSION from 0.0 to 1.25px and drag the window. Foreground
chat-user text and the square badge must remain sharp even while the
backdrop lettering bends under them.
The GitHub workflow publishes real baseline and glass Qt Quick screenshots.

This is an optical prototype only. Apple's compositor and proprietary glass
algorithms are not reproduced; screenshots and a visually strong result must
be verified before integrating the shader into the working Moltorino client.
