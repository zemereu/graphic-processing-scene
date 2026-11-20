#version 410 core

in vec3 fPosition;
in vec3 fNormal;
in vec2 fTexCoords;

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

// adds one light's contribution; lightDirEye points from the fragment towards the light
void addLight(vec3 lightDirEye, vec3 color, float att)
{
    ambient += att * ambientStrength * color;
    diffuse += att * max(dot(normalEye, lightDirEye), 0.0f) * color;

    // Blinn-Phong: half vector between light and view direction instead of the reflection vector
    vec3 halfVector = normalize(lightDirEye + viewDir);
    float specCoeff = pow(max(dot(normalEye, halfVector), 0.0f), shininess);
    specular += att * specularStrength * specCoeff * color;
}

void main()
{
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
    addLight(dirLightEye, lightColor, 1.0f);

    // point lights: direction and attenuation depend on the distance to each lamp
    for (int i = 0; i < numPointLights; i++) {
        vec3 lightPosEye = (view * vec4(pointLightPos[i], 1.0f)).xyz;
        vec3 toLight = lightPosEye - fPosEye;
        float dist = length(toLight);
        float att = 1.0f / (constantAtt + linearAtt * dist + quadraticAtt * dist * dist);
        addLight(toLight / dist, pointLightColor, att);
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
        addLight(lightDirEye, spotLightColor, att * intensity);
    }

    vec3 texColor = texture(diffuseTexture, fTexCoords).rgb;
    vec3 specColor = texture(specularTexture, fTexCoords).rgb;

    vec3 color = (ambient + diffuse) * texColor + specular * specColor + emissiveColor * texColor;
    color = min(color, 1.0f);

    // exponential squared fog: almost clear nearby, then thickens quickly with distance
    float fragmentDistance = length(fPosEye);
    float fogFactor = exp(-pow(fragmentDistance * fogDensity, 2.0f)); // 1 = no fog, 0 = only fog
    color = mix(fogColor, color, clamp(fogFactor, 0.0f, 1.0f));

    fColor = vec4(color, 1.0f);
}