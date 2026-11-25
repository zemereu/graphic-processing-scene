#version 410 core

in vec3 worldPos;
in vec2 texCoords;
in float along;
in vec3 tuftNormal;
in float tint;

out vec4 fColor;

uniform mat4 view;
uniform sampler2D tuftTexture;
uniform vec3 lightDir;
uniform vec3 lightColor;
uniform vec3 ambientColor;
uniform int numPointLights;
uniform vec3 pointLightPos[16];
uniform vec3 pointLightColor;
uniform bool shadowsEnabled;
uniform sampler2DArray shadowMap;
uniform mat4 lightSpaceMatrices[3];
uniform float cascadeSplits[4];

float shadowAt(vec3 p)
{
    if (!shadowsEnabled) return 0.0f;
    float d = -(view * vec4(p, 1.0f)).z;
    if (d >= cascadeSplits[3]) return 0.0f;
    int i = d < cascadeSplits[1] ? 0 : (d < cascadeSplits[2] ? 1 : 2);
    vec3 c = (lightSpaceMatrices[i] * vec4(p, 1.0f)).xyz * 0.5f + 0.5f;
    if (c.z > 1.0f) return 0.0f;
    return c.z - 0.0005f > texture(shadowMap, vec3(c.xy, float(i))).r ? 1.0f : 0.0f;
}

void main()
{
    vec4 texel = texture(tuftTexture, texCoords);
    // the blades are thin: in the small mipmaps their alpha is averaged away and the far tufts would vanish.
    // The alpha is raised with the mipmap level, then made sharp (alpha to coverage turns it into MSAA samples)
    float alpha = texel.a * (1.0f + 0.25f * textureQueryLod(tuftTexture, texCoords).x);
    alpha = clamp((alpha - 0.4f) / max(fwidth(alpha), 0.0001f) + 0.5f, 0.0f, 1.0f);
    if (alpha < 0.01f) discard;

    float diffuse = 0.6f * abs(dot(tuftNormal, lightDir)) + 0.4f * max(lightDir.y, 0.0f);
    vec3 light = ambientColor + lightColor * diffuse * (1.0f - shadowAt(worldPos));
    for (int i = 0; i < numPointLights; i++) {
        float dist = length(pointLightPos[i] - worldPos);
        light += pointLightColor * 0.8f / (1.0f + 0.09f * dist + 0.032f * dist * dist);
    }
    vec3 color = texel.rgb * mix(0.85f, 1.15f, tint) * light * mix(0.55f, 1.0f, along);
    fColor = vec4(color, alpha);
}
