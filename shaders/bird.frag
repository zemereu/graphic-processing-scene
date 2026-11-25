#version 410 core

in float distanceToCamera;
in float underside;

out vec4 fColor;

uniform vec3 lightColor;   // the sun
uniform float dayFactor;   // the sky's light
uniform float fogDensity;

void main()
{
    // dark birds (crows, starlings): mostly silhouettes against the sky, a little lit from above
    vec3 color = vec3(0.06f, 0.06f, 0.07f) * (lightColor * 0.8f + vec3(0.3f) * dayFactor);
    float fog = exp(-pow(distanceToCamera * fogDensity, 2.0f));
    fColor = vec4(color, fog);
}
