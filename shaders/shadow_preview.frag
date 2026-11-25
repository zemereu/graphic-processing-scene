#version 410 core

// one cascade of the shadow map as a grayscale image (UI preview); the depths are stretched for contrast
in vec2 texCoords;
out vec4 fColor;

uniform sampler2DArray shadowMaps;
uniform int layer;

void main()
{
    float d = texture(shadowMaps, vec3(texCoords, float(layer))).r;
    float v = clamp((d - 0.55f) / 0.45f, 0.0f, 1.0f);
    fColor = vec4(vec3(v * v), 1.0f);
}
