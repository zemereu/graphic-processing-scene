#version 410 core

// depth pass: the scene seen from the sun, only the depth is kept (in the shadow map)
layout(location=0) in vec3 vPosition;
layout(location=2) in vec2 vTexCoords;

out vec2 fTexCoords;   // only needed for the alpha test of the foliage

uniform mat4 lightSpaceTrMatrix;
uniform mat4 model;

void main()
{
    fTexCoords = vTexCoords;
    gl_Position = lightSpaceTrMatrix * model * vec4(vPosition, 1.0f);
}
