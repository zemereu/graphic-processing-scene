#version 410 core

// the depth is written automatically; only the transparent parts of the foliage are discarded,
// so the trees cast the shadow of their branches, not of the whole quads
in vec2 fTexCoords;
in vec3 fWorld;

uniform sampler2D diffuseTexture;
// the shadow of a thing that stops being drawn far from the camera fades out before (a dithered shadow map: the
// filtering of the shadows turns it into a lighter shadow); fadeRange.y = 0: no fading
uniform vec2 fadeRange;
uniform vec3 fadeCenter;   // the camera

void main()
{
    if (fadeRange.y > 0.0f) {
        ivec2 p = ivec2(gl_FragCoord.xy) & 3;
        int m[16] = int[16](0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5);
        float threshold = (float(m[p.y * 4 + p.x]) + 0.5f) / 16.0f;
        float keep = 1.0f - clamp((distance(fWorld, fadeCenter) - fadeRange.x) / (fadeRange.y - fadeRange.x), 0.0f, 1.0f);
        if (threshold >= keep) discard;
    }
    float mipLevel = max(textureQueryLod(diffuseTexture, fTexCoords).x, 0.0f);   // same alpha test as basic.frag
    if (texture(diffuseTexture, fTexCoords).a * (1.0f + 0.25f * mipLevel) < 0.5f) {
        discard;
    }
}
