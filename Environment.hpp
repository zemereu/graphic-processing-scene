// What lives and moves in the scene besides the models: the weather (rain, snow, wind, lightning),
// the 3D grass and flowers, the smoke from the power plant and the birds.
#pragma once

#include "App.hpp"

// ==============================================================================================================
// Rain
//
// Rain and snow: line segments (drops) / points (flakes) animated entirely in the vertex shader (shaders/rain.vert),
// blown by the wind; the ground gets wet in the rain (darker, glossy, puddles) or white in the snow (basic.frag).
// ==============================================================================================================

extern bool rainEnabled;
extern bool snowEnabled;      // snow instead of rain (one or the other)
extern float windSpeed;       // meters per second
extern float windDirection;   // degrees, 0 = blowing towards +X
extern gps::Shader rainShader;

// wet ground: grows while it rains (fully wet after WETTING_TIME), dries after it stops (DRYING_TIME)
extern float wetness;   // 0 = dry, 1 = soaked
const float WETTING_TIME = 20.0f;   // seconds
const float DRYING_TIME = 60.0f;

// snow cover: grows while it snows (first in patches, then everywhere facing the sky), melts after it stops
extern float snowCover;   // 0 = none, 1 = everything white
const float SNOW_COVER_TIME = 40.0f;   // seconds
const float SNOW_MELT_TIME = 90.0f;
const float SNOW_FALL_SPEED = 1.5f;    // meters per second

// lightning while it rains: every few seconds a flash (a few quick flickers) lights the whole scene from a random
// direction in the sky, and the sky (and so the fog) lights up around it
extern bool lightningEnabled;
extern glm::vec3 flashDirection;   // towards the flash (world space)
extern glm::vec3 flashColor;       // 0 between flashes
const float LIGHTNING_MIN_DELAY = 4.0f;    // seconds between strikes
const float LIGHTNING_MAX_DELAY = 12.0f;
const glm::vec3 LIGHTNING_COLOR(2.4f, 2.6f, 3.2f);   // cold white, HDR (the bloom makes it glow)

void initRain();
void updateWeather();   // every frame: wetness, snow cover, lightning
void setRain(bool on);  // rain and snow exclude each other
void setSnow(bool on);
void renderRain();      // rain or snow

// ==============================================================================================================
// Grass
//
// 3D grass around the camera: tens of thousands of blades, drawn with instancing from one small blade mesh. The
// blades stand on a grid that moves with the camera in whole grid steps (so they don't slide along the ground);
// each one gets its position offset, height, direction and color from a hash of its grid cell.
// Where grass grows: at startup the whole scene is drawn once from above into a "grass map" (orthographic, top view):
// for every spot, whether the highest surface there is grass ground and how high it is. So there is no grass on the
// roads, the plazas, under the buildings or under the trees, and the blades stand exactly on the ground.
// Farther away, up to ~200 m, the ground is covered by "tufts": two crossed quads with a picture of a tuft of grass,
// of dandelions, of daisies and clover, or of a tall weed (models/grass/tufts.png, made by tools/grass_textures.py).
// The flowers and the weeds also grow among the blades, near the camera.
// The blades bend in the wind, are shadowed by the cascaded shadow maps, lit by the street lamps at night and
// disappear under the snow.
// ==============================================================================================================

extern bool grassEnabled;
extern bool flowersEnabled;   // the flowers and the weeds among the grass
extern bool grassMaskPass;   // true while drawing the grass map
extern gps::Shader grassShader, grassMaskShader, tuftShader;

const int GRASS_GRID = 512;           // blades per side of the grid
const float GRASS_SPACING = 0.2f;     // meters between blades -> a 102 m wide patch around the camera
const int TUFT_GRID = 400;            // tufts per side of their grid
const float TUFT_SPACING = 1.0f;      // meters between tufts -> 200 m around the camera
const int GRASS_MAP_SIZE = 2048;      // the grass map covers the whole world (2 km): ~1 m per texel
const float WORLD_HALF_SIZE = 1000.0f;

void initGrass();     // after the models and the colliders: draws the grass map
void renderGrass();   // after the opaque objects
glm::mat4 grassMapMatrix();   // world -> grass map (top view)

// ==============================================================================================================
// Smoke
//
// Smoke from the power plant: particle systems computed entirely in the vertex shader (like the rain). Each particle
// is a camera-facing quad (billboard) with a random seed; from the time and its seed the shader knows its age, how
// high it has risen, how far the wind has carried it and how big it has grown. Soft round puffs, blended over the
// scene, lit by the sun / moon, fading in the fog.
// ==============================================================================================================

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

// ==============================================================================================================
// Birds
//
// Birds: flocks simulated with Reynolds' boids (1987). Every bird only looks at its neighbours and follows three
// simple rules - separation (don't crowd), alignment (fly the same way), cohesion (stay together) - plus a target
// that wanders over the town; together they give the flock's motion. The birds also keep above the ground and steer
// around the buildings (Collision). Drawn as one small procedural mesh, instanced; the wings flap in the vertex
// shader (flapping while climbing, gliding while descending). At night the birds roost (are not drawn).
// ==============================================================================================================

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
