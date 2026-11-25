#version 410 core

// visible light in the air (volumetric light): along each pixel's view ray, from the camera to the first surface,
// the light that the air scatters towards the camera is added up step by step (ray marching). The floodlights give
// cones of light (with their own shadows: the parts of the cone hidden from the lamp stay dark), the street lamps a
// glow around their bulbs. More with fog, rain or snow in the air.
in vec2 texCoords;
out vec4 fColor;

uniform sampler2D depthTexture;
uniform mat4 invProjection;
uniform mat4 invView;
uniform float density;          // how much light the air scatters (per meter)
uniform float maxDistance;      // meters marched at most
uniform int steps;
// floodlights (as in basic.frag)
uniform int numSpotLights;
uniform vec3 spotLightPos[4];
uniform vec3 spotLightDir[4];
uniform vec3 spotLightColor;
uniform float spotCutOff;
uniform float spotOuterCutOff;
uniform sampler2DArray spotShadowMaps;
uniform mat4 spotLightSpace[4];
uniform vec2 spotShadowNearFar;
// street lamps
uniform int numPointLights;
uniform vec3 pointLightPos[16];
uniform vec3 pointLightColor;

float linearDepth(float d)
{
    float n = spotShadowNearFar.x, f = spotShadowNearFar.y;
    return n * f / (f - d * (f - n));
}

// is the point lit by floodlight i (not hidden behind something)?
float spotVisible(int i, vec3 p)
{
    vec4 l = spotLightSpace[i] * vec4(p, 1.0f);
    if (l.w <= 0.0f) return 0.0f;
    vec3 c = l.xyz / l.w * 0.5f + 0.5f;
    if (any(lessThan(c, vec3(0.0f))) || any(greaterThan(c, vec3(1.0f)))) return 1.0f;
    float closest = linearDepth(texture(spotShadowMaps, vec3(c.xy, float(i))).r);
    return linearDepth(c.z) - 0.3f > closest ? 0.0f : 1.0f;
}

// Henyey-Greenstein phase function: how much light is scattered by an angle (cosTheta: between the light's direction
// and the direction towards the camera); g > 0 = mostly forward, so a beam is brighter when you look towards the lamp
float phase(float cosTheta)
{
    const float g = 0.35f;
    float d = 1.0f + g * g - 2.0f * g * cosTheta;
    return (1.0f - g * g) / (12.566f * d * sqrt(d)) * 12.566f;
}

void main()
{
    float depth = texture(depthTexture, texCoords).r;
    vec4 v = invProjection * vec4(vec3(texCoords, depth) * 2.0f - 1.0f, 1.0f);
    vec3 viewPos = v.xyz / v.w;
    vec3 cameraPos = (invView * vec4(0.0f, 0.0f, 0.0f, 1.0f)).xyz;
    vec3 target = (invView * vec4(viewPos, 1.0f)).xyz;
    vec3 ray = target - cameraPos;
    float rayLength = min(length(ray), maxDistance);
    vec3 dir = normalize(ray);

    // a different starting offset for neighbouring pixels: the step pattern turns into fine noise (blurred after)
    float jitter = fract(52.9829189f * fract(dot(gl_FragCoord.xy, vec2(0.06711056f, 0.00583715f))));
    float stepLength = rayLength / float(steps);
    vec3 light = vec3(0.0f);
    for (int s = 0; s < steps; s++) {
        vec3 p = cameraPos + dir * (float(s) + jitter) * stepLength;
        for (int i = 0; i < numSpotLights; i++) {
            vec3 toPoint = p - spotLightPos[i];
            float dist = length(toPoint);
            vec3 l = toPoint / dist;
            float cone = clamp((dot(l, spotLightDir[i]) - spotOuterCutOff) / (spotCutOff - spotOuterCutOff), 0.0f, 1.0f);
            if (cone <= 0.0f) continue;
            float att = 1.0f / (1.0f + 0.027f * dist + 0.0028f * dist * dist);
            light += spotLightColor * cone * att * phase(dot(l, -dir)) * spotVisible(i, p);
        }
        for (int i = 0; i < numPointLights; i++) {
            vec3 toPoint = p - pointLightPos[i];
            float d2 = dot(toPoint, toPoint);
            if (d2 > 400.0f) continue;   // a lamp's glow only reaches a few meters
            light += pointLightColor * 0.25f / (1.0f + 0.6f * d2) * phase(dot(normalize(toPoint), -dir));
        }
    }
    fColor = vec4(light * density * stepLength, 1.0f);
}
