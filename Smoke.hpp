// Smoke from the power plant: particle systems computed entirely in the vertex shader (like the rain). Each particle
// is a camera-facing quad (billboard) with a random seed; from the time and its seed the shader knows its age, how
// high it has risen, how far the wind has carried it and how big it has grown. Soft round puffs, blended over the
// scene, lit by the sun / moon, fading in the fog.
#pragma once

#include "App.hpp"

struct SmokeEmitter {
    glm::vec3 position;    // world space, center of the opening
    float radius;          // of the opening (meters)
    int particles;
    float lifetime;        // seconds a puff lives
    float riseSpeed;       // meters per second at the start
    float startSize, endSize;   // meters (the puffs grow as they spread)
    glm::vec3 color;
    float opacity;
};

extern bool smokeEnabled;
extern const std::vector<SmokeEmitter> SMOKE_EMITTERS;
extern gps::Shader smokeShader;

void initSmoke();
void renderSmoke();   // after the opaque objects and the sky (blended, no depth writes)
