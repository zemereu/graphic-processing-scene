#version 410 core

// god rays (volumetric light scattering as a post-process, K. Mitchell, GPU Gems 3): from each pixel, steps towards
// the sun on the screen; the bright sky found on the way is added up, fading with every step. Only the sky lets the
// light through (depth = 1), so buildings and trees in front of the sun cut the light into shafts.
in vec2 texCoords;
out vec4 fColor;

uniform sampler2D sceneTexture;
uniform sampler2D depthTexture;
uniform vec2 sunPosition;   // on the screen, 0..1
uniform float intensity;    // 0..1: fades when the sun sets or leaves the screen
uniform float aspect;       // width / height, for round distances on the screen
uniform int samples;

const float DENSITY = 0.9f;   // how far towards the sun the steps go (1 = all the way)
const float DECAY = 0.965f;   // each step counts a little less
const float WEIGHT = 0.06f;

void main()
{
    vec2 step = (texCoords - sunPosition) * DENSITY / float(samples);
    vec2 uv = texCoords;
    float illumination = 1.0f;
    vec3 sum = vec3(0.0f);
    for (int i = 0; i < samples; i++) {
        uv -= step;
        float sky = texture(depthTexture, uv).r >= 0.99999f ? 1.0f : 0.0f;
        vec2 d = (uv - sunPosition) * vec2(aspect, 1.0f);
        float nearSun = exp(-dot(d, d) * 8.0f);   // only the sky around the sun shines through
        sum += min(texture(sceneTexture, uv).rgb, vec3(4.0f)) * sky * nearSun * illumination * WEIGHT;
        illumination *= DECAY;
    }
    fColor = vec4(sum * intensity, 1.0f);
}
