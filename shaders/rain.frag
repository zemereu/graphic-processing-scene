#version 410 core

in float viewDistance;
out vec4 fColor;

uniform vec4 rainColor;     // alpha = how visible the drops are
uniform float areaSize;
uniform bool snow;

void main()
{
    // drops fade out towards the edge of the rain box, so its border is never visible
    float fade = 1.0f - smoothstep(0.25f * areaSize, 0.5f * areaSize, viewDistance);
    if (snow) {
        // a soft round flake
        float r = length(gl_PointCoord * 2.0f - 1.0f);
        if (r > 1.0f) discard;
        fade *= smoothstep(1.0f, 0.3f, r);
    }
    fColor = vec4(rainColor.rgb, rainColor.a * fade);
}
