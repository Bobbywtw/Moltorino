#version 440
layout(location = 0) in vec4 qt_Vertex;
layout(location = 1) in vec2 qt_MultiTexCoord0;
layout(location = 0) out vec2 qt_TexCoord0;

// Uniform block must match the fragment shader exactly on all backends.
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float refractionStrength;
    float vibrancy;
    float specularGloss;
    vec2 panelSize;
    vec2 sourceSize;
    vec2 pointerUV;
    vec2 parallaxPixels;
    float sourceInset;
};
void main()
{
    qt_TexCoord0 = qt_MultiTexCoord0;
    gl_Position = qt_Matrix * qt_Vertex;
}
