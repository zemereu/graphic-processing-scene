#version 410 core

// the grass map: the scene from above (Environment.cpp), x / z -> image, height -> depth
layout(location=0) in vec3 vPosition;
layout(location=2) in vec2 vTexCoords;

out float worldHeight;
out vec2 fTexCoords;

uniform mat4 mapMatrix;
uniform mat4 model;

void main()
{
    vec4 world = model * vec4(vPosition, 1.0f);
    worldHeight = world.y;
    fTexCoords = vTexCoords;
    gl_Position = mapMatrix * world;
}
