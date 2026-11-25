#version 410 core

// red = 1 where the highest surface is grass ground, green = its height
in float worldHeight;
in vec2 fTexCoords;

out vec4 fColor;

uniform bool grass;
uniform sampler2D diffuseTexture;

void main()
{
    // the transparent parts of the foliage don't cover the ground (same alpha test as basic.frag)
    if (texture(diffuseTexture, fTexCoords).a < 0.5f) discard;
    fColor = vec4(grass ? 1.0f : 0.0f, worldHeight, 0.0f, 1.0f);
}
