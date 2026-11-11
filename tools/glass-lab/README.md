# Moltorino Qt Quick glass lab — optical surface test

This is an isolated Qt 6.9 shader lab, not an actual Moltorino chat build.
The app uses `QQuickView` and compiled `.qsb` vertex and fragment shaders,
with actual in-app texture sampling from a backdrop **outside the glass
foreground's capture tree**. There is no desktop or other-window capture.

## Revision 3 — stronger material test

- Neutral graphite scene with a detailed 24px grid, distinct concentric circles,
  sharp typography and two inherited accent colors to reveal color vibrancy
- Rounded rectangle signed-distance meniscus with a 26px edge shell;
  maximum displaced pixels are visibly bent at edges while the center stays crisp
- Nine-tap approximation for backdrop diffusion (not multi-pass Gaussian)
- Saturation adjustable from 1.0 to 1.65; default 1.45 is a visual choice,
  **not a known or verifiable Apple material parameter**
- Actual cursor-reactive rim reflections and a separate glossy ridge above composer
- Clickable refraction, vibrancy and edge light values for easy live tuning
- **LENS OFF** uses the same geometry and background but bypasses all
  refraction, blur, vibrancy and specular material calculations in the shader
- Drag motion drives a short easing parallax displacement (no desktop capture)

## How to inspect

Run `MoltorinoGlassLab.exe` from the dedicated build artifact. Compare ON
against OFF. Wave the cursor near a panel perimeter and drag the window.
The GitHub workflow publishes real baseline and glass Qt Quick screenshots.

This is an optical prototype only. Apple's compositor and proprietary glass
algorithms are not reproduced; screenshots and a visually strong result must
be verified before integrating the shader into the working Moltorino client.
