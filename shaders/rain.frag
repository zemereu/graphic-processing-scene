#version 410 core

in float viewDistance;
out vec4 fColor;

uniform vec4 rainColor;     // alpha = how visible the drops are
uniform float areaSize;

void main()
{
    // drops fade out towards the edge of the rain box, so its border is never visible
    float fade = 1.0f - smoothstep(0.25f * areaSize, 0.5f * areaSize, viewDistance);
    fColor = vec4(rainColor.rgb, rainColor.a * fade);
}
