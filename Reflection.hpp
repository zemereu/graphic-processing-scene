// Water reflections (planar reflection): before the normal pass, the scene is drawn a second time as seen in a mirror
// laid on the water surface (the view is mirrored across the plane y = WATER_LEVEL; whatever is under the water is
// cut off with a clip plane) into a half-resolution texture. The water shader reads it at its own pixel on the
// screen, shifted a little by the waves.
#pragma once

#include "App.hpp"

extern bool reflectionsEnabled;
extern bool reflectionPass;   // true while drawing the mirrored scene
const float WATER_LEVEL = -2.0f;
const int REFLECTION_TEXTURE_UNIT = 9;

void initReflection();                                // after initShaders()
void createReflectionBuffers(int width, int height);  // with the scene framebuffers
void deleteReflectionBuffers();
void renderReflection();   // the mirrored scene, into the reflection texture
void bindReflection();     // the reflection texture, for the main shader (water)
