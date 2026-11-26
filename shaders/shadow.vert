#version 410 core

// depth pass: the scene seen from the sun, only the depth is kept (in the shadow map)
layout(location=0) in vec3 vPosition;
layout(location=2) in vec2 vTexCoords;

out vec2 fTexCoords;   // only needed for the alpha test of the foliage
out vec3 fWorld;       // for the fading out with the distance from the camera

uniform mat4 lightSpaceTrMatrix;
uniform mat4 model;

// wind in the trees (swayHeight = 0 for every other object): (same as basic.vert, so the shadows sway too): the tree bends away from the wind, more and more towards
// the top (the trunk stays planted), in gusts; the branches also flutter faster, more at their tips
uniform vec3 swayWind;     // model space: direction of the wind * how far the top of the tree leans (model units)
uniform float swayHeight;  // height of the tree (model units)
uniform float swayPhase;   // different for every tree, so they don't all move together
uniform float time;        // seconds

vec3 windSway(vec3 p)
{
    if (swayHeight <= 0.0f) return p;
    float h = clamp(p.y / swayHeight, 0.0f, 1.0f);
    float gust = 1.0f + 0.5f * sin(time * 1.1f + swayPhase) + 0.25f * sin(time * 2.3f + swayPhase * 1.7f);
    float flutter = 0.15f * sin(time * 5.0f + swayPhase + p.y * 0.8f + p.x * 1.3f) * length(p.xz) / 3.0f;
    return p + swayWind * (h * h * gust + h * flutter);
}


// flags (flagWave = 0 for every other object): the cloth streams away from the pole (+x), waves run along it, faster
// and bigger in a strong wind; in a calm it hangs down along the pole. n: the bent normal (model space)
uniform float flagWave;    // the wind speed at the flag (m/s); 0 = not a flag
uniform float flagLength;  // model units

vec3 flagCloth(vec3 p, out vec3 n)
{
    float u = clamp(p.x / flagLength, 0.0f, 1.0f);
    float s = clamp(flagWave / 8.0f, 0.0f, 1.0f);              // 0 = calm, 1 = streaming
    float droop = radians(80.0f) * (1.0f - s) * (1.0f - s);    // how far it hangs down
    float k = 6.2832f / (0.9f * flagLength);
    float phase = k * p.x - (2.5f + 0.6f * flagWave) * time + 0.8f * p.y;
    float a = 0.03f + 0.05f * s;
    float z = flagLength * a * u * sin(phase);
    float dzdx = a * sin(phase) + flagLength * a * u * k * cos(phase);
    float dzdy = flagLength * a * u * 0.8f * cos(phase);
    float c = cos(droop), sn = sin(droop);
    n = normalize(vec3(-dzdx, -dzdy, 1.0f));
    n = vec3(n.x * c + n.y * sn, -n.x * sn + n.y * c, n.z);
    return vec3(p.x * c + p.y * sn, -p.x * sn + p.y * c, z);
}

void main()
{
    fTexCoords = vTexCoords;
    vec3 position = windSway(vPosition);
    vec3 n;
    if (flagWave > 0.0f) position = flagCloth(vPosition, n);
    fWorld = (model * vec4(position, 1.0f)).xyz;
    gl_Position = lightSpaceTrMatrix * vec4(fWorld, 1.0f);
}
