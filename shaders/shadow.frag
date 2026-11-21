#version 410 core

// the depth is written automatically; only the transparent parts of the foliage are discarded,
// so the trees cast the shadow of their branches, not of the whole quads
in vec2 fTexCoords;

uniform sampler2D diffuseTexture;

void main()
{
    float mipLevel = max(textureQueryLod(diffuseTexture, fTexCoords).x, 0.0f);   // same alpha test as basic.frag
    if (texture(diffuseTexture, fTexCoords).a * (1.0f + 0.25f * mipLevel) < 0.5f) {
        discard;
    }
}
