#version 410 core

// the skybox: a unit cube around the camera; each vertex position is also the direction it shows
layout(location=0) in vec3 vPosition;

out vec3 skyDir;

uniform mat4 view;         // rotation only
uniform mat4 projection;

void main()
{
    skyDir = vPosition;
    vec4 p = projection * view * vec4(vPosition, 1.0f);
    gl_Position = p.xyww;   // z = w -> depth 1 after the division: behind every object
}
