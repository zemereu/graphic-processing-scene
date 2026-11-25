#version 410 core

// one smoke puff: a camera-facing quad. Everything follows from the time and the puff's random seed (no state kept
// on the CPU): its age in its cycle, how high it has risen, how far the wind carried it, how big it has grown.
layout(location=0) in vec2 corner;   // -1..1
layout(location=1) in vec4 seed;     // per puff, 0..1

out vec2 puffCoord;   // -1..1 across the puff
out float age;        // 0 = just out of the chimney, 1 = gone
out float seedOut;
out float distanceToCamera;

uniform mat4 view;
uniform mat4 projection;
uniform float time;
uniform vec3 source;
uniform float sourceRadius;
uniform float lifetime;
uniform float riseSpeed;
uniform float startSize;
uniform float endSize;
uniform vec2 wind;   // m/s, world xz

void main()
{
    age = fract(time / lifetime + seed.x);   // the puffs are spread evenly over the cycle
    float t = age * lifetime;
    vec2 start = (seed.yz * 2.0f - 1.0f) * sourceRadius;
    vec3 p = source + vec3(start.x, 0.0f, start.y);
    p.y += riseSpeed * t * (1.0f - 0.4f * age);           // rises, slower and slower as it cools
    p.xz += wind * t * (0.3f + 0.7f * age);               // the higher, the more the wind carries it
    p += vec3(sin(t * 0.7f + seed.w * 20.0f), 0.3f * sin(t * 0.5f + seed.y * 10.0f),
              cos(t * 0.6f + seed.y * 20.0f)) * endSize * 0.25f * age;   // turbulence
    float size = mix(startSize, endSize, sqrt(age));

    // billboard: the corners along the camera's right / up axes; each puff turned by its own slowly changing angle
    float a = seed.w * 6.2831853f + t * 0.15f;
    vec2 c = mat2(cos(a), sin(a), -sin(a), cos(a)) * corner;
    vec3 right = vec3(view[0][0], view[1][0], view[2][0]);
    vec3 up = vec3(view[0][1], view[1][1], view[2][1]);
    vec3 world = p + (right * c.x + up * c.y) * size;

    puffCoord = corner;
    seedOut = seed.w;
    vec4 eye = view * vec4(world, 1.0f);
    distanceToCamera = length(eye.xyz);
    gl_Position = projection * eye;
}
