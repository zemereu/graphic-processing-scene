#version 410 core

in vec3 skyDir;

out vec4 fColor;

uniform float fogDensity;   // 0 = no fog

// the two skies (same functions as in basic.frag): the sunset photo and the night photo, blended by daylight
// (twilight in between), each turned around the vertical so the glow of the sun / moon in it follows the light
uniform samplerCube daySky;
uniform samplerCube nightSky;
uniform float daylight;            // 1 = day, 0 = night
uniform float daySkyAngle;         // radians
uniform float nightSkyAngle;
uniform float nightSkyBrightness;
uniform vec3 sunDirection;         // the real sun (world space), also below the horizon
uniform vec3 flashDirection;       // lightning: towards the flash (world space)
uniform vec3 flashColor;           // 0 between flashes  // the night photo is exposed for the stars: darker in the scene

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

// haze: the sky just above the horizon in this direction, blurred
vec3 hazeColor(vec3 dir)
{
    vec2 h = dot(dir.xz, dir.xz) > 1e-6f ? normalize(dir.xz) : vec2(0.0f, 1.0f);
    return skyColor(vec3(h.x, 0.06f, h.y), 5.0f);
}

void main()
{
    vec3 dir = normalize(skyDir);
    vec3 color = skyColor(dir, 0.0f);

    // below the horizon the photos show their own water; the scene has its terrain there, so it is hidden in the haze.
    // With fog, the haze also rises a little above the horizon, like the fog on the distant objects.
    float top = fogDensity > 0.0f ? 0.12f : 0.01f;
    float haze = 1.0f - smoothstep(-0.01f, top, dir.y);
    color = mix(color, hazeColor(dir), haze);

    fColor = vec4(color, 1.0f);
}
