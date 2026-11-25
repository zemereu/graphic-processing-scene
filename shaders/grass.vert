#version 410 core

// one blade of grass (instanced): which blade it is (gl_InstanceID) gives its cell in a grid around the camera; the
// cell gives, through a hash, the blade's offset, height, direction and shade; the grass map says if it grows there
// and how high the ground is
layout(location=0) in vec2 bladeVertex;   // x = across the blade (-1..1), y = along it (0 = root, 1 = tip)

out vec3 worldPos;
out float along;
out vec3 bladeColor;
out vec3 bladeNormal;

uniform mat4 view;
uniform mat4 projection;
uniform vec3 cameraPos;
uniform float time;
uniform vec2 wind;              // m/s, world xz
uniform int grid;
uniform float spacing;
uniform float worldHalfSize;
uniform float snowCover;
uniform sampler2D grassMap;

float hash(vec2 p)
{
    return fract(sin(dot(p, vec2(127.1f, 311.7f))) * 43758.5453f);
}

// smooth value noise: patches of taller and shorter grass, a few tens of meters wide (the same in grass_tuft.vert)
float patchNoise(vec2 p)
{
    vec2 c = floor(p), f = fract(p);
    f = f * f * (3.0f - 2.0f * f);
    return mix(mix(hash(c), hash(c + vec2(1.0f, 0.0f)), f.x), mix(hash(c + vec2(0.0f, 1.0f)), hash(c + 1.0f), f.x), f.y);
}

void main()
{
    int i = gl_InstanceID % grid;
    int j = gl_InstanceID / grid;
    // the grid moves with the camera in whole steps: each blade stays on its spot of the ground
    vec2 cell = floor(cameraPos.xz / spacing) * spacing + (vec2(i, j) - 0.5f * float(grid)) * spacing;
    float h1 = hash(cell), h2 = hash(cell + 17.31f), h3 = hash(cell + 41.73f), h4 = hash(cell + 73.11f);
    vec2 pos = cell + (vec2(h1, h2) - 0.5f) * spacing;

    vec2 map = textureLod(grassMap, pos / (2.0f * worldHalfSize) + 0.5f, 0.0f).rg;
    float radius = 0.5f * float(grid) * spacing;
    float dist = length(vec3(pos.x, map.g, pos.y) - cameraPos);   // 3D: seen from high up, the blades fade out
    // shorter towards the edge of the patch (no visible border), under the snow; none where it's not grass
    float height = mix(0.25f, 0.6f, h3) * mix(0.4f, 1.0f, patchNoise(pos * 0.08f)) * (1.0f - smoothstep(0.6f * radius, radius, dist))
                 * (1.0f - snowCover) * step(0.5f, map.r)
                 * step(abs(pos.x), worldHalfSize) * step(abs(pos.y), worldHalfSize);   // not past the map's edge
    if (height < 0.02f) {
        gl_Position = vec4(2.0f, 2.0f, 2.0f, 1.0f);   // outside the view: nothing is drawn
        return;
    }

    float yaw = h4 * 6.2831853f;
    vec3 facing = vec3(cos(yaw), 0.0f, sin(yaw));
    vec3 across = vec3(-facing.z, 0.0f, facing.x);
    float t = bladeVertex.y;
    vec3 p = vec3(pos.x, map.g, pos.y) + across * bladeVertex.x * 0.03f + vec3(0.0f, t * height, 0.0f);
    // the blade curves a little, and bends with the wind in gusts that roll over the field
    p += facing * t * t * height * 0.4f * (h1 - 0.3f);
    float gust = 0.6f + 0.4f * sin(time * 2.1f + pos.x * 0.35f + pos.y * 0.27f);
    p.xz += wind * 0.03f * gust * t * t * height;

    worldPos = p;
    along = t;
    bladeColor = mix(vec3(0.16f, 0.26f, 0.05f), vec3(0.30f, 0.42f, 0.09f), h2);
    bladeColor = mix(bladeColor, vec3(0.42f, 0.38f, 0.16f), 0.35f * h3 * h3);   // a few dry ones
    bladeNormal = normalize(facing + vec3(0.0f, 0.7f, 0.0f));
    gl_Position = projection * view * vec4(p, 1.0f);
}
