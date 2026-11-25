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
#pragma once

#include "App.hpp"

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
