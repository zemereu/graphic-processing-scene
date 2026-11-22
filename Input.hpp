// Keyboard and mouse: the window callbacks, moving the camera (W A S D + mouse) and moving / rotating / scaling
// Palatul Telefoanelor (arrows, Q / E, Z / X, R).
#pragma once

#include "App.hpp"

// mouse
extern bool firstMouse;
extern float yaw, pitch;

void setWindowCallbacks();
void processMovement();         // the camera, every frame
void processObjectTransform();  // Palatul Telefoanelor, every frame
