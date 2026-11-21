#version 410 core

in vec3 fPosition;
in vec3 fNormal;
in vec2 fTexCoords;
in vec4 fragPosLightSpace;

out vec4 fColor;

//matrices
uniform mat4 model;
uniform mat4 view;
uniform mat3 normalMatrix;
// directional light (sun by day, moon by night)
uniform vec3 lightDir;
uniform vec3 lightColor;
// point lights (street lamps), positions in world space
#define MAX_POINT_LIGHTS 16
uniform int numPointLights;
uniform vec3 pointLightPos[MAX_POINT_LIGHTS];
uniform vec3 pointLightColor;
// spot lights (floodlights), position and direction in world space
#define MAX_SPOT_LIGHTS 4
uniform int numSpotLights;
uniform vec3 spotLightPos[MAX_SPOT_LIGHTS];
uniform vec3 spotLightDir[MAX_SPOT_LIGHTS];
uniform vec3 spotLightColor;
uniform float spotCutOff;       // cosine of the inner cone angle (full intensity inside)
uniform float spotOuterCutOff;  // cosine of the outer cone angle (no light outside)
// light emitted by the object itself (lamp bulbs at night)
uniform vec3 emissiveColor;
// textures
uniform sampler2D diffuseTexture;
uniform sampler2D specularTexture;
// view mode
uniform bool flatShading;
// shadows of the directional light
uniform sampler2D shadowMap;
uniform bool shadowsEnabled;
// fog
uniform float fogDensity;   // 0 = no fog
uniform vec3 fogColor;      // same as the sky, so distant objects blend into it

const float ambientStrength = 0.2f;
const float specularStrength = 0.5f;
const float shininess = 32.0f;
// point light attenuation for a range of ~50 m (Lab 8, table 1)
const float constantAtt = 1.0f;
const float linearAtt = 0.09f;
const float quadraticAtt = 0.032f;
// spot light attenuation for a range of ~160 m (Lab 8, table 1)
const float spotLinearAtt = 0.027f;
const float spotQuadraticAtt = 0.0028f;

vec3 fPosEye;
vec3 normalEye;
vec3 viewDir;

vec3 ambient = vec3(0.0f);
vec3 diffuse = vec3(0.0f);
vec3 specular = vec3(0.0f);

// adds one light's contribution; lightDirEye points from the fragment towards the light;
// shadow (0 = lit, 1 = in shadow) removes the direct light, the ambient part stays
void addLight(vec3 lightDirEye, vec3 color, float att, float shadow)
{
    ambient += att * ambientStrength * color;
    diffuse += (1.0f - shadow) * att * max(dot(normalEye, lightDirEye), 0.0f) * color;

    // Blinn-Phong: half vector between light and view direction instead of the reflection vector
    vec3 halfVector = normalize(lightDirEye + viewDir);
    float specCoeff = pow(max(dot(normalEye, halfVector), 0.0f), shininess);
    specular += (1.0f - shadow) * att * specularStrength * specCoeff * color;
}

// shadow mapping: is something closer to the sun than this fragment, along the sun's direction?
float computeShadow(vec3 lightDirEye)
{
    if (!shadowsEnabled) return 0.0f;

    // position seen from the sun -> shadow map coordinates in [0, 1]
    vec3 p = fragPosLightSpace.xyz / fragPosLightSpace.w;
    p = p * 0.5f + 0.5f;
    if (p.z > 1.0f) return 0.0f;

    // bias against "shadow acne" (a surface shadowing itself), larger on surfaces at an angle to the sun
    float bias = max(0.0008f * (1.0f - dot(normalEye, lightDirEye)), 0.0002f);

    // PCF: average of 3x3 neighbouring tests -> soft shadow edges instead of jagged ones
    float shadow = 0.0f;
    vec2 texelSize = 1.0f / vec2(textureSize(shadowMap, 0));
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            float closestDepth = texture(shadowMap, p.xy + vec2(x, y) * texelSize).r;
            shadow += (p.z - bias > closestDepth) ? 1.0f : 0.0f;
        }
    }
    shadow /= 9.0f;

    // the shadow map only covers an area around the camera: fade the shadows out towards its edge
    float edge = max(abs(p.x - 0.5f), abs(p.y - 0.5f)) * 2.0f;
    return shadow * (1.0f - smoothstep(0.8f, 1.0f, edge));
}

void main()
{
    // alpha test: the foliage (branch cards, tree billboards) is drawn on quads with transparent texture parts
    // (the smaller mipmaps average the alpha towards 0, so it is scaled up with the mip level, otherwise
    //  the distant trees would disappear)
    vec4 texSample = texture(diffuseTexture, fTexCoords);
    float mipLevel = max(textureQueryLod(diffuseTexture, fTexCoords).x, 0.0f);
    if (texSample.a * (1.0f + 0.25f * mipLevel) < 0.5f) {
        discard;
    }

    // eye space: the camera is at the origin
    fPosEye = (view * model * vec4(fPosition, 1.0f)).xyz;
    // flat: face normal from position derivatives (constant across the triangle)
    // smooth: normal interpolated between vertices
    normalEye = flatShading
        ? normalize(cross(dFdx(fPosEye), dFdy(fPosEye)))
        : normalize(normalMatrix * fNormal);
    viewDir = normalize(-fPosEye);

    // directional light: same direction for every fragment, no attenuation
    vec3 dirLightEye = normalize((view * vec4(lightDir, 0.0f)).xyz);
    addLight(dirLightEye, lightColor, 1.0f, computeShadow(dirLightEye));

    // point lights: direction and attenuation depend on the distance to each lamp
    for (int i = 0; i < numPointLights; i++) {
        vec3 lightPosEye = (view * vec4(pointLightPos[i], 1.0f)).xyz;
        vec3 toLight = lightPosEye - fPosEye;
        float dist = length(toLight);
        float att = 1.0f / (constantAtt + linearAtt * dist + quadraticAtt * dist * dist);
        addLight(toLight / dist, pointLightColor, att, 0.0f);
    }

    // spot lights: like point lights, but only inside a cone around the light direction
    for (int i = 0; i < numSpotLights; i++) {
        vec3 lightPosEye = (view * vec4(spotLightPos[i], 1.0f)).xyz;
        vec3 spotDirEye = normalize((view * vec4(spotLightDir[i], 0.0f)).xyz);
        vec3 toLight = lightPosEye - fPosEye;
        float dist = length(toLight);
        vec3 lightDirEye = toLight / dist;

        // angle between the spot direction and the direction towards this fragment;
        // smooth falloff between the inner and the outer cone instead of a hard edge
        float theta = dot(-lightDirEye, spotDirEye);
        float intensity = clamp((theta - spotOuterCutOff) / (spotCutOff - spotOuterCutOff), 0.0f, 1.0f);

        float att = 1.0f / (constantAtt + spotLinearAtt * dist + spotQuadraticAtt * dist * dist);
        addLight(lightDirEye, spotLightColor, att * intensity, 0.0f);
    }

    vec3 texColor = texSample.rgb;
    vec3 specColor = texture(specularTexture, fTexCoords).rgb;

    vec3 color = (ambient + diffuse) * texColor + specular * specColor + emissiveColor * texColor;
    color = min(color, 1.0f);

    // exponential squared fog: almost clear nearby, then thickens quickly with distance
    float fragmentDistance = length(fPosEye);
    float fogFactor = exp(-pow(fragmentDistance * fogDensity, 2.0f)); // 1 = no fog, 0 = only fog
    color = mix(fogColor, color, clamp(fogFactor, 0.0f, 1.0f));

    fColor = vec4(color, 1.0f);
}
