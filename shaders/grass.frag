#version 410 core

in vec3 worldPos;
in float along;
in vec3 bladeColor;
in vec3 bladeNormal;

out vec4 fColor;

uniform mat4 view;
uniform vec3 lightDir;       // towards the sun / moon (world space)
uniform vec3 lightColor;
uniform vec3 ambientColor;
// street lamps
uniform int numPointLights;
uniform vec3 pointLightPos[16];
uniform vec3 pointLightColor;
// shadows: the cascades of the main pass (same as basic.frag), one sample is enough for a blade of grass
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
    // thin blades let light through: lit from either side, and a little from above whatever their direction
    float diffuse = 0.6f * abs(dot(bladeNormal, lightDir)) + 0.4f * max(lightDir.y, 0.0f);
    vec3 light = ambientColor + lightColor * diffuse * (1.0f - shadowAt(worldPos));
    for (int i = 0; i < numPointLights; i++) {
        float dist = length(pointLightPos[i] - worldPos);
        light += pointLightColor * 0.8f / (1.0f + 0.09f * dist + 0.032f * dist * dist);
    }
    // darker at the root, where the blades shade each other
    vec3 color = bladeColor * light * mix(0.55f, 1.0f, along);
    fColor = vec4(color, 1.0f);
}
