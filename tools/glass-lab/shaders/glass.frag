#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

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
layout(binding = 1) uniform sampler2D source;

float roundedBox(vec2 p, vec2 b, float r)
{
    vec2 q = abs(p) - b + vec2(r);
    return length(max(q, vec2(0.0))) + min(max(q.x, q.y), 0.0) - r;
}

void main()
{
    vec2 size = max(panelSize, vec2(2.0));
    vec2 uv = qt_TexCoord0;
    vec2 p = uv * size - 0.5 * size;
    float r = min(14.0, size.y * 0.47);
    float d = roundedBox(p, 0.5 * size - vec2(0.6), r);
    float coverage = 1.0 - smoothstep(-1.15, 1.15, d);

    vec2 edge = 1.0 - smoothstep(vec2(0.0), vec2(0.24), min(uv, 1.0 - uv));
    vec2 lens = sign(uv - 0.5) * edge * vec2(6.0, 4.0)
              * clamp(refractionStrength, 0.0, 2.0);

    // The ShaderEffectSource samples only an isolated in-app canvas.
    // The source texture includes an inset, so displaced samples do not
    // wrap to opposite sides of the pane.
    vec2 texCoord = (uv * size + vec2(sourceInset) + lens + parallaxPixels)
                  / max(sourceSize, vec2(1.0));
    texCoord = clamp(texCoord, vec2(0.003), vec2(0.997));
    vec2 texel = 1.6 / max(sourceSize, vec2(1.0));

    vec3 sharp = texture(source, texCoord).rgb;
    vec3 blurred =
        texture(source, texCoord).rgb * 0.32 +
        texture(source, clamp(texCoord + vec2(texel.x, 0.0), 0.0, 1.0)).rgb * 0.17 +
        texture(source, clamp(texCoord - vec2(texel.x, 0.0), 0.0, 1.0)).rgb * 0.17 +
        texture(source, clamp(texCoord + vec2(0.0, texel.y), 0.0, 1.0)).rgb * 0.17 +
        texture(source, clamp(texCoord - vec2(0.0, texel.y), 0.0, 1.0)).rgb * 0.17;

    vec3 transmitted = mix(sharp, blurred, 0.53);
    float luma = dot(transmitted, vec3(0.2126, 0.7152, 0.0722));
    transmitted = clamp(mix(vec3(luma), transmitted,
                            clamp(vibrancy, 1.0, 1.5)), 0.0, 1.0);

    float rim = exp(-abs(d) * 0.85);
    float upper = pow(1.0 - clamp(uv.y * 2.6, 0.0, 1.0), 2.0);
    float glint = exp(-length((uv - pointerUV) * vec2(2.0, 4.0)) * 5.0);
    float shimmer = rim * (0.18 * upper + 0.23 * glint)
                  * clamp(specularGloss, 0.0, 1.0);
    vec3 color = transmitted * 0.91 + vec3(0.032, 0.036, 0.044)
               + vec3(shimmer) + vec3(0.022) * rim;

    float alpha = coverage * 0.97;
    // ShaderEffect uses premultiplied alpha for source-over blending.
    fragColor = vec4(clamp(color, 0.0, 1.0) * alpha, alpha) * qt_Opacity;
}
