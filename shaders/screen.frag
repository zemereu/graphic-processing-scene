#version 410 core

in vec2 texCoords;
out vec4 fColor;

// the antialiased scene (linear colors); the sRGB framebuffer encodes them when writing
uniform sampler2D screenTexture;

void main()
{
    fColor = vec4(texture(screenTexture, texCoords).rgb, 1.0f);
}
