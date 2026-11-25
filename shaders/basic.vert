#version 410 core

layout(location=0) in vec3 vPosition;
layout(location=1) in vec3 vNormal;
layout(location=2) in vec2 vTexCoords;

out vec3 fPosition;
out vec3 fNormal;
out vec2 fTexCoords;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
// water reflections: the mirrored scene keeps only what is above the water (clip distance < 0 = cut off)
uniform vec4 clipPlane;

// wind in the trees (swayHeight = 0 for every other object): the tree bends away from the wind, more and more towards
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
	vec3 position = windSway(vPosition);
	gl_Position = projection * view * model * vec4(position, 1.0f);
	fPosition = position;
	fNormal = vNormal;
	fTexCoords = vTexCoords;
	gl_ClipDistance[0] = dot(model * vec4(position, 1.0f), clipPlane);
}
