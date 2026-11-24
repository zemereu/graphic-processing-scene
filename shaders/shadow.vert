#version 410 core

// depth pass: the scene seen from the sun, only the depth is kept (in the shadow map)
layout(location=0) in vec3 vPosition;
layout(location=2) in vec2 vTexCoords;

out vec2 fTexCoords;   // only needed for the alpha test of the foliage

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

void main()
{
    fTexCoords = vTexCoords;
    gl_Position = lightSpaceTrMatrix * model * vec4(windSway(vPosition), 1.0f);
}
