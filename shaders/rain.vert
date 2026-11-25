#version 410 core

// one raindrop = one line segment (2 vertices); for snow, one flake = one point (only the first vertex)
// seed.xyz: random numbers in [0, 1) that place the drop in the rain box
// seed.w:   0 for the bottom end of the streak, 1 for the top end
layout(location = 0) in vec4 seed;

uniform mat4 view;
uniform mat4 projection;
uniform vec3 cameraPos;
uniform float time;        // seconds
uniform float fallSpeed;   // meters per second
uniform vec2 wind;         // horizontal wind velocity (x, z), meters per second
uniform float areaSize;    // width/depth of the rain box around the camera
uniform float areaHeight;  // height of the rain box
uniform float dropLength;  // length of a streak, meters
uniform bool snow;         // snowflakes: slow, fluttering points instead of streaks

out float viewDistance;

void main()
{
    // the drops never move on the CPU: their position is a function of time,
    // and the box wraps around the camera, so it always rains wherever you go
    vec3 velocity = vec3(wind.x, -fallSpeed, wind.y);
    vec3 p = vec3(seed.x * areaSize, seed.y * areaHeight, seed.z * areaSize) + velocity * time;

    // wrap every coordinate into a box centered on the camera
    p.x = cameraPos.x + mod(p.x - cameraPos.x, areaSize) - 0.5f * areaSize;
    p.y = cameraPos.y + mod(p.y - cameraPos.y, areaHeight) - 0.5f * areaHeight;
    p.z = cameraPos.z + mod(p.z - cameraPos.z, areaSize) - 0.5f * areaSize;

    if (snow) {
        // flakes flutter around their path, each with its own rhythm
        p.x += 0.6f * sin(time * 1.3f + seed.x * 40.0f);
        p.z += 0.6f * cos(time * 1.1f + seed.z * 40.0f);
    } else {
        // the top end trails behind the drop, along its velocity -> slanted streaks in the wind
        p -= normalize(velocity) * dropLength * seed.w;
    }

    vec4 posEye = view * vec4(p, 1.0f);
    viewDistance = length(posEye.xyz);
    gl_Position = projection * posEye;
    if (snow) {
        gl_PointSize = clamp(40.0f / viewDistance, 1.5f, 7.0f);   // nearer flakes look bigger
        if (seed.w > 0.5f) gl_Position = vec4(2.0f, 2.0f, 2.0f, 1.0f);   // the second vertex is not needed
    }
}
