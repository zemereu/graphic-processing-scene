#version 410 core

in vec2 puffCoord;
in float age;
in float seedOut;
in float distanceToCamera;

out vec4 fColor;

uniform vec3 smokeColor;
uniform float opacity;
uniform vec3 lightColor;    // the sun / moon
uniform float dayFactor;    // 1 = day: the sky lights the smoke too
uniform float fogDensity;

float hash(vec2 p)
{
    return fract(sin(dot(p, vec2(127.1f, 311.7f))) * 43758.5453f);
}

float valueNoise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0f - 2.0f * f);
    return mix(mix(hash(i), hash(i + vec2(1.0f, 0.0f)), u.x),
               mix(hash(i + vec2(0.0f, 1.0f)), hash(i + vec2(1.0f, 1.0f)), u.x), u.y);
}

void main()
{
    // soft round puff with a cloudy (noise) inside
    float r = length(puffCoord);
    if (r > 1.0f) discard;
    vec2 q = puffCoord * 2.5f + seedOut * 17.0f;
    float cloud = 0.6f * valueNoise(q) + 0.4f * valueNoise(q * 2.3f);
    float density = smoothstep(1.0f, 0.2f, r) * mix(0.5f, 1.0f, cloud);
    // fades in out of the chimney, thins out as it spreads
    density *= smoothstep(0.0f, 0.08f, age) * (1.0f - smoothstep(0.5f, 1.0f, age));
    // the fog hides it with distance, like the objects
    float fog = exp(-pow(distanceToCamera * fogDensity, 2.0f));
    float alpha = density * opacity * fog;

    // lit by the sun / moon and by the sky; the top of a puff a little brighter than its bottom
    vec3 light = lightColor * (0.75f + 0.25f * puffCoord.y) + vec3(0.25f) * dayFactor + vec3(0.02f);
    fColor = vec4(smokeColor * light, alpha);
}
