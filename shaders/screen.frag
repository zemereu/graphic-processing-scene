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
    // (the framebuffer is sRGB: the power makes the fade even to the eye)
    fColor = vec4(acesFilm(hdr * e) * pow(1.0f - fade, 2.2f), 1.0f);
}
