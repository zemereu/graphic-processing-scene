#version 410 core

in vec2 texCoords;
out vec4 fColor;

// the antialiased scene in linear HDR colors (values above 1 for the lamps, the sun...); the sRGB framebuffer encodes
// the result when writing
uniform sampler2D screenTexture;
uniform sampler2D bloomTexture;     // the blurred bright parts of the scene
uniform sampler2D godRaysTexture;   // light shafts from the sun
uniform sampler2D ssaoTexture;      // ambient occlusion: 1 = open, 0 = occluded
uniform float bloomStrength;        // 0 = off
uniform float godRaysStrength;
uniform float ssaoStrength;
uniform float exposure;            // fixed exposure
uniform sampler2D lightBeamsTexture;   // light scattered in the air (floodlights, lamps)
uniform float lightBeamsStrength;
uniform bool autoExposure;
uniform sampler2D adaptedTexture;  // 1x1: the adapted average log luminance
uniform float exposureKey;         // the average luminance the image is brought to
uniform vec2 exposureLimits;
uniform float fade;                // the tours' fades: 0 = the image, 1 = black
uniform float gloom;               // the "Russian winter" filter: 0 = off, 1 = full
uniform float grainSeed;           // changes every frame: the film grain moves

// the grim winter look: the colors drained out, cold blue-green shadows, a dull yellowish grey in the light, the
// blacks lifted and the whites dimmed (a flat, matte image), dark corners, film grain. On the display colors.
vec3 gloomGrade(vec3 c)
{
    float lum = dot(c, vec3(0.299f, 0.587f, 0.114f));
    c = mix(vec3(lum), c, 0.12f);                                   // almost no color left
    c *= mix(vec3(0.74f, 0.86f, 0.90f), vec3(0.96f, 0.95f, 0.86f), smoothstep(0.15f, 0.85f, lum));  // cold, sick light
    c *= 0.82f;                                                     // a dim day
    c = mix(vec3(0.07f, 0.08f, 0.085f), vec3(0.78f, 0.79f, 0.78f), c);   // no black, no white: flat, matte
    c = mix(c, vec3(dot(c, vec3(0.333f))), 0.15f);
    float r = length((texCoords - 0.5f) * vec2(1.3f, 1.0f));
    c *= mix(0.45f, 1.0f, smoothstep(0.8f, 0.2f, r));               // heavy dark corners
    float n = fract(sin(dot(texCoords * 1000.0f + grainSeed, vec2(12.9898f, 78.233f))) * 43758.5453f) - 0.5f;
    return clamp(c + n * 0.06f, 0.0f, 1.0f);
}

// tone mapping: HDR -> [0, 1] with a filmic curve (ACES, fitted by K. Narkowicz): the highlights roll off smoothly
// instead of being clipped flat, the dark tones get a little more contrast
vec3 acesFilm(vec3 x)
{
    return clamp((x * (2.51f * x + 0.03f)) / (x * (2.43f * x + 0.59f) + 0.14f), 0.0f, 1.0f);
}

void main()
{
    float ao = mix(1.0f, texture(ssaoTexture, texCoords).r, ssaoStrength);
    vec3 hdr = texture(screenTexture, texCoords).rgb * ao
             + bloomStrength * texture(bloomTexture, texCoords).rgb
             + godRaysStrength * texture(godRaysTexture, texCoords).rgb
             + lightBeamsStrength * texture(lightBeamsTexture, texCoords).rgb;
    float e = exposure;
    if (autoExposure) {
        float averageLuminance = exp(texelFetch(adaptedTexture, ivec2(0), 0).r);
        e = clamp(exposureKey / averageLuminance, exposureLimits.x, exposureLimits.y);
    }
    vec3 color = acesFilm(hdr * e);
    if (gloom > 0.0f) {   // graded on the display colors (the framebuffer encodes to sRGB after)
        vec3 display = pow(color, vec3(1.0f / 2.2f));
        color = pow(mix(display, gloomGrade(display), gloom), vec3(2.2f));
    }
    // (the framebuffer is sRGB: the power makes the fade even to the eye)
    fColor = vec4(color * pow(1.0f - fade, 2.2f), 1.0f);
}
