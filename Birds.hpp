// Birds: flocks simulated with Reynolds' boids (1987). Every bird only looks at its neighbours and follows three
// simple rules - separation (don't crowd), alignment (fly the same way), cohesion (stay together) - plus a target
// that wanders over the town; together they give the flock's motion. The birds also keep above the ground and steer
// around the buildings (Collision). Drawn as one small procedural mesh, instanced; the wings flap in the vertex
// shader (flapping while climbing, gliding while descending). At night the birds roost (are not drawn).
#pragma once

#include "App.hpp"

struct Bird {
    glm::vec3 position;
    glm::vec3 velocity;
    float phase;    // of the wing beat (radians)
    float flap;     // 0 = gliding, 1 = flapping
    int flock;
};

extern bool birdsEnabled;
extern std::vector<Bird> birds;
extern gps::Shader birdShader;

const int BIRD_FLOCKS = 3;
const int BIRDS_PER_FLOCK = 40;
const float BIRD_MIN_SPEED = 7.0f;    // m/s
const float BIRD_MAX_SPEED = 13.0f;
const float BIRD_NEIGHBOUR_RADIUS = 12.0f;
const float BIRD_SEPARATION_RADIUS = 3.0f;

void initBirds();
void updateBirds();   // every frame: one step of the simulation
void renderBirds();   // after the opaque objects and the sky
