#version 410 core

// bloom (glow around very bright pixels), in two steps drawn on the fullscreen triangle (screen.vert):
//   mode 0 - bright pass: keeps only what is brighter than the threshold (soft knee, no hard edge)
//   mode 1 - blur: separable Gaussian, 9 taps along one direction; applied back and forth several times
in vec2 texCoords;
out vec4 fColor;

uniform sampler2D image;
uniform int mode;
uniform vec2 direction;     // blur: one texel along x or along y
uniform float threshold;    // bright pass: linear brightness where the glow starts

const float weights[5] = float[](0.227027f, 0.1945946f, 0.1216216f, 0.054054f, 0.016216f);

void main()
{
    if (mode == 0) {
        vec3 c = texture(image, texCoords).rgb;
        float brightness = max(c.r, max(c.g, c.b));
        float knee = 0.5f * threshold;
        float soft = clamp(brightness - threshold + knee, 0.0f, 2.0f * knee);
        soft = soft * soft / (4.0f * knee + 1e-4f);
        float contribution = max(soft, brightness - threshold) / max(brightness, 1e-4f);
        fColor = vec4(c * contribution, 1.0f);
    } else {
        vec3 sum = texture(image, texCoords).rgb * weights[0];
        for (int i = 1; i < 5; i++) {
            sum += texture(image, texCoords + direction * float(i)).rgb * weights[i];
            sum += texture(image, texCoords - direction * float(i)).rgb * weights[i];
        }
        fColor = vec4(sum, 1.0f);
    }
}
