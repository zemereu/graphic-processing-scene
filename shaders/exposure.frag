#version 410 core

// automatic exposure (eye adaptation):
//   mode 0 - the log luminance of every pixel (the mipmaps of this image then give its average)
//   mode 1 - the adapted luminance (1x1): moves a little every frame from the last value towards the average of
//            the current image, faster when it gets brighter (eyes adapt to light faster than to darkness)
in vec2 texCoords;
out vec4 fColor;

uniform sampler2D image;          // mode 0: the scene; mode 1: the log luminance (mipmapped)
uniform sampler2D previous;       // mode 1: the adapted value of the last frame
uniform int mode;
uniform float lowestMip;
uniform float deltaTime;
uniform bool snap;                // take the new value at once (start-up)

void main()
{
    if (mode == 0) {
        vec3 c = texture(image, texCoords).rgb;
        float luminance = dot(c, vec3(0.2126f, 0.7152f, 0.0722f));
        fColor = vec4(log(max(luminance, 1e-4f)), 0.0f, 0.0f, 1.0f);
        return;
    }
    float current = textureLod(image, vec2(0.5f), lowestMip).r;   // average log luminance
    float last = texelFetch(previous, ivec2(0), 0).r;
    float speed = current > last ? 2.0f : 0.8f;
    float adapted = snap ? current : last + (current - last) * (1.0f - exp(-deltaTime * speed));
    fColor = vec4(adapted, 0.0f, 0.0f, 1.0f);
}
