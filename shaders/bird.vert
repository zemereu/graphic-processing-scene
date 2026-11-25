#version 410 core

// one bird of a flock (instanced): the mesh is turned to fly along its velocity, and its wings beat
layout(location=0) in vec4 vertex;    // xyz = model space (x = right wing, z = forward), w = 0 body .. 1 wing tip
layout(location=1) in vec3 birdPosition;
layout(location=2) in vec3 birdForward;
layout(location=3) in vec2 wingBeat;  // x = phase (radians), y = 0 gliding .. 1 flapping

out float distanceToCamera;
out float underside;   // 1 = seen from below (darker)

uniform mat4 view;
uniform mat4 projection;

void main()
{
    vec3 p = vertex.xyz;
    // the wing bends up and down around the body; more towards the tip
    float angle = (0.15f + 0.6f * sin(wingBeat.x)) * wingBeat.y + 0.1f * (1.0f - wingBeat.y);
    float span = abs(p.x);
    p.y += span * sin(angle) * (0.6f + 0.4f * vertex.w);
    p.x *= cos(angle);

    // the bird's own axes: forward = its velocity, up = as close to the world's up as possible
    vec3 forward = normalize(birdForward);
    vec3 right = normalize(cross(forward, vec3(0.0f, 1.0f, 0.0f)));
    vec3 up = cross(right, forward);
    vec3 world = birdPosition + right * p.x + up * p.y + forward * p.z;

    vec4 eye = view * vec4(world, 1.0f);
    distanceToCamera = length(eye.xyz);
    underside = eye.y > (view * vec4(birdPosition, 1.0f)).y ? 0.0f : 1.0f;
    gl_Position = projection * eye;
}
