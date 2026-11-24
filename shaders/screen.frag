#version 410 core

in vec2 texCoords;
out vec4 fColor;

// the antialiased scene in linear HDR colors (values above 1 for the lamps, the sun...); the sRGB framebuffer encodes
// the result when writing
uniform sampler2D screenTexture;
uniform sampler2D bloomTexture;   // the blurred bright parts of the scene
uniform float bloomStrength;      // 0 = no bloom
uniform float exposure;

// tone mapping: HDR -> [0, 1] with a filmic curve (ACES, fitted by K. Narkowicz): the highlights roll off smoothly
// instead of being clipped flat, the dark tones get a little more contrast
vec3 acesFilm(vec3 x)
{
    return clamp((x * (2.51f * x + 0.03f)) / (x * (2.43f * x + 0.59f) + 0.14f), 0.0f, 1.0f);
}

void main()
{
    vec3 hdr = texture(screenTexture, texCoords).rgb + bloomStrength * texture(bloomTexture, texCoords).rgb;
    fColor = vec4(acesFilm(hdr * exposure), 1.0f);
}
