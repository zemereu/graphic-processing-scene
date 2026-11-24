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
// light from the sky itself (ambient only): keeps the scene lit at dusk, when the sun is too low to light it
uniform vec3 skyLight;
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
uniform sampler2D normalTexture;   // tangent space, OpenGL convention (flat for the objects without one)

// 0 = normal object, 1 = terrain (two texture scales mixed: hides the repetition of the tiles over large areas),
// 2 = water (two normal maps sliding in different directions = moving waves, reflects the sky at grazing angles)
uniform int surfaceMode;
uniform float time;     // seconds, for the waves
// view mode
uniform bool flatShading;
// shadows of the directional light
uniform sampler2D shadowMap;
uniform bool shadowsEnabled;
// shadows of the floodlights: one layer per spot light, perspective projections
uniform sampler2DArray spotShadowMaps;
uniform mat4 spotLightSpace[MAX_SPOT_LIGHTS];
uniform int numSpotShadows;          // 0 = no floodlight shadows
uniform vec2 spotShadowNearFar;      // near / far planes of their projections (meters)
// rain: 0 = dry, 1 = soaked (grows while it rains, dries slowly after; Rain.cpp)
uniform float wetness;
// fog
uniform float fogDensity;   // 0 = no fog
// the skies (see skybox.frag): give the fog its color and are what the water reflects
uniform samplerCube daySky;
uniform samplerCube nightSky;
uniform float daylight;            // 1 = day, 0 = night
uniform float daySkyAngle;         // radians
uniform float nightSkyAngle;
uniform float nightSkyBrightness;
uniform vec3 sunDirection;         // the real sun (world space), also below the horizon
uniform vec3 flashDirection;       // lightning: towards the flash (world space)
uniform vec3 flashColor;           // 0 between flashes

const float ambientStrength = 0.2f;
const float EMISSIVE_INTENSITY = 2.5f;   // light-emitting surfaces, in HDR
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

// shininess / strength of the highlights of this fragment: higher on wet surfaces
float matShininess = shininess;
float matSpecular = specularStrength;

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
    float specCoeff = pow(max(dot(normalEye, halfVector), 0.0f), surfaceMode == 2 ? 300.0f : matShininess);
    specular += (1.0f - shadow) * att * matSpecular * specCoeff * color;
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

// the sky in a direction (world space): the two photos blended by daylight, each turned so the glow of the
// sun / moon in it follows the light (same functions as in skybox.frag)
vec3 rotateY(vec3 d, float angle)
{
    float c = cos(angle), s = sin(angle);
    return vec3(c * d.x - s * d.z, d.y, s * d.x + c * d.z);
}

vec3 skyColor(vec3 dir, float lod)
{
    vec3 day = textureLod(daySky, rotateY(dir, daySkyAngle), lod).rgb;
    // the photo stops at white, the real sun is far brighter: an HDR sun disk + halo where the sun really is, so it
    // (and its reflection in the water) glows through the bloom; gone once it sets; not in the blurred haze
    if (lod < 0.5f) {
        float s = max(dot(dir, sunDirection), 0.0f);
        float visible = smoothstep(-0.02f, 0.02f, sunDirection.y);
        day += visible * vec3(1.0f, 0.85f, 0.6f) * (8.0f * pow(s, 4000.0f) + 0.6f * pow(s, 300.0f));
    }
    vec3 night = textureLod(nightSky, rotateY(dir, nightSkyAngle), lod).rgb * nightSkyBrightness;
    // lightning: the clouds light up, most around the flash
    vec3 flash = flashColor * (0.15f + 0.6f * pow(max(dot(dir, flashDirection), 0.0f), 6.0f));
    return mix(night, day, daylight) + flash;
}

// haze: the sky just above the horizon in this direction, blurred.
// The fog takes this color, so a distant object fades into exactly the sky behind it.
vec3 hazeColor(vec3 dir)
{
    vec2 h = dot(dir.xz, dir.xz) > 1e-6f ? normalize(dir.xz) : vec2(0.0f, 1.0f);
    return skyColor(vec3(h.x, 0.06f, h.y), 5.0f);
}

// value noise (smoothly interpolated random values on a grid): where the puddles are
float hash(vec2 p)
{
    return fract(sin(dot(p, vec2(127.1f, 311.7f))) * 43758.5453f);
}

float valueNoise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0f - 2.0f * f);
    return mix(mix(hash(i), hash(i + vec2(1.0f, 0.0f)), u.x),
               mix(hash(i + vec2(0.0f, 1.0f)), hash(i + vec2(1.0f, 1.0f)), u.x), u.y);
}

// normal mapping without tangents in the vertex data: the tangent frame is rebuilt per pixel from the screen-space
// derivatives of the position and of the texture coordinates ("cotangent frame", C. Schueler, ShaderX5)
vec3 perturbNormal(vec3 N, vec3 p, vec2 uv, vec3 mapNormal)
{
    vec3 dp1 = dFdx(p);
    vec3 dp2 = dFdy(p);
    vec2 duv1 = dFdx(uv);
    vec2 duv2 = dFdy(uv);
    vec3 dp2perp = cross(dp2, N);
    vec3 dp1perp = cross(N, dp1);
    vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;
    float len = max(dot(T, T), dot(B, B));
    if (len < 1e-20f) {
        return N;
    }
    float invmax = inversesqrt(len);
    return normalize(mat3(T * invmax, B * invmax, N) * mapNormal);
}

// perspective depth -> distance from the light along its axis (meters)
float linearDepth(float d)
{
    float n = spotShadowNearFar.x, f = spotShadowNearFar.y;
    return n * f / (f - d * (f - n));
}

// floodlight i: is something between it and this fragment? The depths are compared as distances in meters, so the
// same bias works near and far (the perspective depth buffer is much finer near the light than far from it)
float computeSpotShadow(int i, vec3 worldPos, vec3 lightDirEye)
{
    if (i >= numSpotShadows) return 0.0f;
    vec4 p = spotLightSpace[i] * vec4(worldPos, 1.0f);
    if (p.w <= 0.0f) return 0.0f;                    // behind the light
    vec3 c = p.xyz / p.w * 0.5f + 0.5f;
    if (c.z > 1.0f) return 0.0f;
    float current = linearDepth(c.z);
    float bias = 0.05f + 0.4f * (1.0f - max(dot(normalEye, lightDirEye), 0.0f)) + 0.002f * current;
    float shadow = 0.0f;
    vec2 texelSize = 1.0f / vec2(textureSize(spotShadowMaps, 0).xy);
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            float closest = linearDepth(texture(spotShadowMaps, vec3(c.xy + vec2(x, y) * texelSize, float(i))).r);
            shadow += (current - bias > closest) ? 1.0f : 0.0f;
        }
    }
    return shadow / 9.0f;
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
    vec3 worldPos = (model * vec4(fPosition, 1.0f)).xyz;
    // flat: face normal from position derivatives (constant across the triangle)
    // smooth: normal interpolated between vertices
    normalEye = flatShading
        ? normalize(cross(dFdx(fPosEye), dFdy(fPosEye)))
        : normalize(normalMatrix * fNormal);
    viewDir = normalize(-fPosEye);

    // normal map (only in smooth mode: the flat mode shows the polygons)
    vec2 uv = fTexCoords;
    if (!flatShading) {
        vec3 mapNormal;
        if (surfaceMode == 2) {
            vec3 n1 = texture(normalTexture, uv + time * vec2(0.031f, 0.017f)).xyz * 2.0f - 1.0f;
            vec3 n2 = texture(normalTexture, uv * 1.73f + vec2(0.4f, 0.2f) - time * vec2(0.012f, -0.026f)).xyz * 2.0f - 1.0f;
            mapNormal = normalize(vec3((n1.xy + n2.xy) * 1.6f, n1.z * n2.z));
        } else {
            mapNormal = texture(normalTexture, uv).xyz * 2.0f - 1.0f;
            // far away the details are smaller than a pixel: fade the normal map out (less shimmering)
            mapNormal.xy *= 1.0f - 0.7f * smoothstep(60.0f, 400.0f, length(fPosEye));
        }
        normalEye = perturbNormal(normalEye, fPosEye, uv, normalize(mapNormal));
    }

    // rain: what faces the sky gets fully wet, the walls only a little; on the hard surfaces (not on the terrain: the
    // soil drinks it) water gathers in puddles in the hollows (value noise): still water, a flat mirror.
    // Wet surfaces are darker and glossy (sharp highlights, reflect the sky)
    float wet = 0.0f, puddle = 0.0f;
    if (wetness > 0.0f && surfaceMode != 2) {
        vec3 normalWorld = transpose(mat3(view)) * normalEye;
        float up = smoothstep(0.6f, 0.95f, normalWorld.y);
        wet = wetness * mix(0.3f, 1.0f, up);
        vec2 ground = worldPos.xz;
        float n = 0.65f * valueNoise(ground * 0.22f) + 0.35f * valueNoise(ground * 0.9f);
        if (surfaceMode == 0) {
            puddle = up * smoothstep(0.64f, 0.69f, n) * smoothstep(0.4f, 1.0f, wetness);
        }
        normalEye = normalize(mix(normalEye, normalize((view * vec4(0.0f, 1.0f, 0.0f, 0.0f)).xyz), puddle));
        matShininess = mix(shininess, 250.0f, max(wet, puddle));
        matSpecular = specularStrength * (1.0f + 2.0f * max(wet, puddle));
    }

    // the sky's own light, then the directional light: same direction for every fragment, no attenuation
    ambient += skyLight;
    vec3 dirLightEye = normalize((view * vec4(lightDir, 0.0f)).xyz);
    addLight(dirLightEye, lightColor, 1.0f, computeShadow(dirLightEye));

    // lightning: a directional light for a split second (no shadow map for it)
    if (dot(flashColor, flashColor) > 0.0f) {
        addLight(normalize((view * vec4(flashDirection, 0.0f)).xyz), flashColor, 1.0f, 0.0f);
    }

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
        float spotShadow = intensity > 0.0f ? computeSpotShadow(i, worldPos, lightDirEye) : 0.0f;
        addLight(lightDirEye, spotLightColor, att * intensity, spotShadow);
    }

    vec3 texColor = texSample.rgb;
    if (surfaceMode == 1) {
        // terrain: the same texture at a ~5x larger scale on top: the tiles stop repeating visibly in the distance
        vec3 macro = texture(diffuseTexture, fTexCoords * 0.19f + vec2(0.37f, 0.71f)).rgb;
        float w = 0.3f + 0.3f * smoothstep(30.0f, 400.0f, length(fPosEye));
        texColor = mix(texColor, macro, w);
    }
    vec3 specColor = texture(specularTexture, fTexCoords).rgb;
    if (wet > 0.0f) {
        texColor *= mix(1.0f, 0.55f, wet) * mix(1.0f, 0.6f, puddle);   // water fills the pores: darker
        specColor = mix(specColor, vec3(1.0f), max(wet, puddle) * 0.7f);
    }

    vec3 lit = (ambient + diffuse) * texColor;
    if (wet > 0.0f) {
        // the wet film / the puddles reflect the sky (Fresnel: much more at grazing angles)
        float fresnel = 0.04f + 0.96f * pow(1.0f - max(dot(normalEye, viewDir), 0.0f), 5.0f);
        vec3 reflected = transpose(mat3(view)) * reflect(-viewDir, normalEye);
        reflected.y = abs(reflected.y);
        float amount = clamp(fresnel, 0.0f, 1.0f) * (0.35f * wet + 0.5f * puddle) + 0.1f * puddle;
        lit = mix(lit, 0.8f * skyColor(reflected, 1.0f), clamp(amount, 0.0f, 0.8f));
    }
    if (surfaceMode == 2) {
        // water reflects the sky, more and more towards grazing angles (Schlick's Fresnel approximation);
        // the reflected direction is taken back to world space to read the skybox
        float fresnel = 0.03f + 0.97f * pow(1.0f - max(dot(normalEye, viewDir), 0.0f), 5.0f);
        vec3 reflected = transpose(mat3(view)) * reflect(-viewDir, normalEye);
        reflected.y = abs(reflected.y);   // the waves can tilt it downwards; the water only sees the sky
        vec3 skyReflection = skyColor(reflected, 1.0f);
        lit = mix(lit, skyReflection * 0.8f, clamp(fresnel, 0.0f, 0.6f));
    }
    // no clamping to 1: the scene is HDR, the tone mapping (screen.frag) brings it to the screen; the glowing parts
    // (lit windows, lamp bulbs) are brighter than white, so the bloom makes them shine
    vec3 color = lit + specular * specColor + EMISSIVE_INTENSITY * emissiveColor * texColor;

    // exponential squared fog: almost clear nearby, then thickens quickly with distance
    float fragmentDistance = length(fPosEye);
    float fogFactor = exp(-pow(fragmentDistance * fogDensity, 2.0f)); // 1 = no fog, 0 = only fog
    vec3 viewDirWorld = transpose(mat3(view)) * (fPosEye / fragmentDistance);   // camera -> fragment
    color = mix(hazeColor(viewDirWorld), color, clamp(fogFactor, 0.0f, 1.0f));

    fColor = vec4(color, 1.0f);
}
