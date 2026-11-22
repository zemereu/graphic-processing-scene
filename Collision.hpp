// Collision detection: the camera is a sphere that can't enter the buildings' boxes and stays above the ground
// (terrain height map written by tools/gen_world.py).
#pragma once

#include "App.hpp"

extern bool collisionsEnabled;

void initColliders();     // the boxes of the meshes (model space) and the height map, once at startup
void updateColliders();   // the boxes in world space, every frame (some objects move)
float groundHeight(float x, float z);
bool collides(const glm::vec3& center);
void moveCamera(gps::MOVE_DIRECTION direction, float distance);
