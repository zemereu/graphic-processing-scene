#version 410 core

// one tuft (instanced): two crossed quads with a picture of a tuft of grass, a few flowers or a tall weed. Same idea as
// the blades (grass.vert): a grid that moves with the camera, a hash of the cell for everything else, the grass map for
// where it grows. The tufts reach much farther than the blades: near the camera only the flowers and the weeds show
// (among the blades), farther away the tufts of grass take over from the blades, up to the edge of the grid.
layout(location=0) in vec3 tuftVertex;   // x = across the quad (-1..1), y = up (0..1), z = which quad (0 / 1)

out vec3 worldPos;
out vec2 texCoords;
out float along;
out vec3 tuftNormal;
out float tint;

uniform mat4 view;
uniform mat4 projection;
uniform vec3 cameraPos;
uniform float time;
uniform vec2 wind;
uniform int grid;
uniform float spacing;
uniform float worldHalfSize;
uniform float snowCover;
uniform float bladeRadius;      // where the blades end
uniform bool flowersEnabled;
uniform sampler2D grassMap;

float hash(vec2 p)
{
    return fract(sin(dot(p, vec2(127.1f, 311.7f))) * 43758.5453f);
}

// smooth value noise: patches of taller and shorter grass, a few tens of meters wide (the same in grass.vert)
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
    vec2 cell = floor(cameraPos.xz / spacing) * spacing + (vec2(i, j) - 0.5f * float(grid)) * spacing;
    float h1 = hash(cell + 3.7f), h2 = hash(cell + 23.9f), h3 = hash(cell + 57.1f), h4 = hash(cell + 91.3f);
    vec2 pos = cell + (vec2(h1, h2) - 0.5f) * spacing;
    vec2 map = textureLod(grassMap, pos / (2.0f * worldHalfSize) + 0.5f, 0.0f).rg;
    float dist = length(vec3(pos.x, map.g, pos.y) - cameraPos);
    float radius = 0.5f * float(grid) * spacing;

    // which picture: 0 = grass, 1 = dandelions, 2 = daisies and clover, 3 = tall weed
    float type = 0.0f;
    if (flowersEnabled) type = h3 < 0.85f ? 0.0f : (h3 < 0.91f ? 1.0f : (h3 < 0.97f ? 2.0f : 3.0f));
    float size;
    if (type == 0.0f) {
        // the tufts of grass come in where the blades go away (grass.vert: from 0.6 to 1 of their radius)
        size = smoothstep(0.55f * bladeRadius, 0.9f * bladeRadius, dist);
    } else {
        // the flowers and the weeds are everywhere, but sparse near the camera (they stand out among the blades)
        size = h4 < 0.4f ? 1.0f : smoothstep(0.55f * bladeRadius, 0.9f * bladeRadius, dist);
    }
    size *= (1.0f - smoothstep(0.7f * radius, radius, dist)) * (1.0f - snowCover) * step(0.5f, map.r);
    if (size < 0.02f) {
        gl_Position = vec4(2.0f, 2.0f, 2.0f, 1.0f);   // not drawn
        return;
    }
    // far away the tufts grow a little: fewer pixels each, but the ground stays covered
    size *= mix(0.75f, 1.25f, h4) * (1.0f + 0.3f * smoothstep(60.0f, 200.0f, dist));
    // the grass as high as the blades around (grass.vert), the flowers a little above, the weeds the highest
    float grassHeight = mix(0.4f, 1.0f, patchNoise(pos * 0.08f));
    float height = (type == 0.0f ? 0.45f * grassHeight : (type == 3.0f ? 0.8f : 0.5f * max(grassHeight, 0.7f))) * size;
    float width = (type == 3.0f ? 0.35f : 0.4f) * size;

    float yaw = h2 * 6.2831853f + (tuftVertex.z > 0.5f ? 1.5707963f : 0.0f);
    vec3 across = vec3(cos(yaw), 0.0f, sin(yaw));
    float t = tuftVertex.y;
    vec3 p = vec3(pos.x, map.g, pos.y) + across * tuftVertex.x * width + vec3(0.0f, t * height, 0.0f);
    float gust = 0.6f + 0.4f * sin(time * 2.1f + pos.x * 0.35f + pos.y * 0.27f);
    p.xz += wind * 0.03f * gust * t * t * height;

    worldPos = p;
    along = t;
    // the picture of the tile, kept a little away from its sides (the mipmaps blend in the next tile)
    texCoords = vec2((type + clamp(tuftVertex.x * 0.5f + 0.5f, 0.01f, 0.99f)) * 0.25f, t);
    tuftNormal = normalize(vec3(-across.z, 0.0f, across.x) + vec3(0.0f, 0.7f, 0.0f));
    tint = h1;
    gl_Position = projection * view * vec4(p, 1.0f);
}
