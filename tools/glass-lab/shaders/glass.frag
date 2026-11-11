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
    float lensEnabled;
};
layout(binding = 1) uniform sampler2D source;

// Exact signed distance to a rounded-rectangle panel.
float roundedBox(vec2 p, vec2 halfSize, float radius)
{
    vec2 q = abs(p) - halfSize + vec2(radius);
    return length(max(q, vec2(0.0))) + min(max(q.x, q.y), 0.0) - radius;
}

vec3 sampleBackdrop(vec2 pixels)
{
    vec2 uv = clamp(pixels / max(sourceSize, vec2(1.0)),
                    vec2(0.001), vec2(0.999));
    return texture(source, uv).rgb;
}

void main()
{
    vec2 size = max(panelSize, vec2(2.0));
    vec2 uv = qt_TexCoord0;
    vec2 centered = uv * size - size * 0.5;
    float radius = min(19.0, size.y * 0.45);
    float signedDistance = roundedBox(centered, size * 0.5 - vec2(0.9), radius);
    float coverage = 1.0 - smoothstep(-1.25, 1.25, signedDistance);

    // OFF deliberately uses the same real background and panel geometry,
    // but removes all lensing, saturation, blur, and specular reflections.
    if (lensEnabled < 0.5)
    {
        float flatAlpha = coverage * 0.20;
        fragColor = vec4(vec3(0.052) * flatAlpha, flatAlpha) * qt_Opacity;
        return;
    }

    float depthFromEdge = max(0.0, -signedDistance);
    // A 31px meniscus shell bends the backdrop without folding it over.
    // Softer curvature prevents the hooked duplicate lines seen in build #3.
    float shell = 1.0 - smoothstep(0.0, 31.0, depthFromEdge);
    float meniscus = pow(clamp(shell, 0.0, 1.0), 1.8);

    // Analytic outward normal for straight edges and rounded corners.
    vec2 inner = max(size * 0.5 - vec2(radius), vec2(1.0));
    vec2 normalVector = centered - clamp(centered, -inner, inner);
    float normalLength = length(normalVector);
    vec2 outward = normalLength > 0.0001
                   ? normalVector / normalLength : vec2(0.0, -1.0);

    // Genuine UV displacement of sampled backdrop pixels, rather than
    // merely a translucent overlay or a bright outline.
    float strength = clamp(refractionStrength, 0.0, 1.8);
    vec2 meniscusOffset = -outward * meniscus * 9.0 * strength;
    vec2 basePixels = uv * size + vec2(sourceInset) + parallaxPixels;
    vec2 refractedPixels = basePixels + meniscusOffset;

    vec3 sharp = sampleBackdrop(refractedPixels);

    // Compact nine-tap Gaussian-style diffusion; not claimed as a
    // true multi-pass Gaussian blur. Radius is within source padding.
    const float blurRadius = 3.0;
    vec2 hx = vec2(blurRadius, 0.0);
    vec2 hy = vec2(0.0, blurRadius);
    vec3 diffusion =
        sharp * 0.25 +
        sampleBackdrop(refractedPixels + hx) * 0.125 +
        sampleBackdrop(refractedPixels - hx) * 0.125 +
        sampleBackdrop(refractedPixels + hy) * 0.125 +
        sampleBackdrop(refractedPixels - hy) * 0.125 +
        sampleBackdrop(refractedPixels + hx + hy) * 0.0625 +
        sampleBackdrop(refractedPixels + hx - hy) * 0.0625 +
        sampleBackdrop(refractedPixels - hx + hy) * 0.0625 +
        sampleBackdrop(refractedPixels - hx - hy) * 0.0625;

    float diffusionMix = mix(0.25, 0.71, meniscus);
    vec3 transmitted = mix(sharp, diffusion, diffusionMix);

    // Neutral transmission: only the underlying pixels contribute color.
    float luma = dot(transmitted, vec3(0.2126, 0.7152, 0.0722));
    transmitted = clamp(mix(vec3(luma), transmitted,
                            clamp(vibrancy, 1.0, 1.65)), 0.0, 1.0);

    // A very thin bright outer rim and a separate inner bevel shadow give
    // this shape optical thickness instead of a neon-rectangle appearance.
    float outsideRim = exp(-pow((signedDistance + 0.55) / 1.22, 2.0));
    float innerBevel = exp(-pow((depthFromEdge - 4.8) / 3.9, 2.0));
    float innerCatch = exp(-pow((depthFromEdge - 2.0) / 2.25, 2.0));

    // Cursor position affects light direction and where it gleams along rim.
    bool pointerValid = all(greaterThanEqual(pointerUV, vec2(0.0))) &&
                        all(lessThanEqual(pointerUV, vec2(1.0)));
    vec2 lightDir = pointerValid
                    ? normalize(pointerUV - vec2(0.5) + vec2(0.00001))
                    : normalize(vec2(-0.55, -0.82));
    float faceLight = max(0.0, dot(outward, -lightDir));
    vec2 edgePos = uv;
    float nearCursor = pointerValid
        ? exp(-pow(length((edgePos - pointerUV) * vec2(1.2, 1.8)) * 3.4, 2.0))
        : 0.0;
    float gloss = clamp(specularGloss, 0.0, 3.0);
    float specular = gloss * outsideRim *
                     (0.095 + 0.16 * pow(faceLight, 3.0) + 0.25 * nearCursor);
    float secondaryRim = gloss * innerCatch * (0.023 + 0.042 * faceLight);

    vec3 result = transmitted * (1.0 - innerBevel * 0.17);
    result += vec3(specular + secondaryRim);
    // Compensate transmission only near the perimeter; no blue tint.
    result += vec3(meniscus * 0.015);

    fragColor = vec4(clamp(result, 0.0, 1.0) * coverage, coverage)
                * qt_Opacity;
}
