#version 410 core

// fullscreen triangle generated from gl_VertexID (no vertex buffer needed):
// vertices (0,0), (2,0), (0,2) in texture space cover the whole screen
out vec2 texCoords;

void main()
{
    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    texCoords = p;
    gl_Position = vec4(p * 2.0f - 1.0f, 0.0f, 1.0f);
}
