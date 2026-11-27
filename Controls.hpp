// Everything the user controls: keyboard and mouse, camera collisions, the camera tour and the
// control panel.
#pragma once

#include "App.hpp"

// ==============================================================================================================
// Input
//
// Keyboard and mouse: the window callbacks, moving the camera (W A S D + mouse) and moving / rotating / scaling
// Palatul Telefoanelor (arrows, Q / E, Z / X, R).
// ==============================================================================================================

// mouse
extern bool firstMouse;
extern float yaw, pitch;

void setWindowCallbacks();
void processMovement();         // the camera, every frame
void processObjectTransform();  // Palatul Telefoanelor, every frame

// ==============================================================================================================
// Collision
//
// Collision detection: the camera is a sphere that can't enter the buildings' boxes and stays above the ground
// (terrain height map written by tools/gen_world.py).
// ==============================================================================================================

extern bool collisionsEnabled;

void initColliders();     // the boxes of the meshes (model space) and the height map, once at startup
void updateColliders();   // the boxes in world space, every frame (some objects move)
float groundHeight(float x, float z);
bool collides(const glm::vec3& center);
void moveCamera(gps::MOVE_DIRECTION direction, float distance);

// ==============================================================================================================
// Tour
//
// Camera tour: the camera follows a smooth path (Catmull-Rom spline) through key points, at a roughly constant speed.
// ==============================================================================================================

// The landmarks tour: still shots of the landmarks, from one to the next through black (fade out, cut, fade in).
// Both play sounds/kletka.mp3 (if it's there) while they run.
extern bool tourActive;
extern bool shotsTour;        // the landmarks tour is the one running
extern float tourTime;
extern float screenFade;      // 0: the image as it is, 1: black
extern std::vector<float> tourKeyTimes;   // time at which the camera reaches each key point

void initTour();
void startTour(bool shots);   // shots: the landmarks tour; else the flight
float tourLength();           // seconds
void stopTour();
void updateTour();

// ==============================================================================================================
// UI
//
// The control panel (Dear ImGui): FPS, view mode, lighting, fog, weather, traffic, rendering options, camera tour,
// the keyboard-controlled building and the key map.
// ==============================================================================================================

extern bool showUI;
extern bool sceneReady;   // false while loading: the callbacks only remember the window size, ignore the keys

void initUI();       // after setWindowCallbacks(): ImGui chains to our callbacks
void renderUI();
void shutdownUI();
void showLoading(float progress, const char* what);   // the loading screen: one frame with a progress bar
