// Post-processing: what is done to the finished (resolved) image before it reaches the window, using its colors and
// its depth buffer. All in linear HDR colors, mostly at half resolution:
//   SSAO      ambient occlusion: corners, window recesses and the foot of the walls get darker
//   god rays  light shafts from the sun through the gaps between buildings and trees (radial blur of the sky)
//   light beams  light scattered in the air: the floodlights' cones and the street lamps' glow (ray marching)
//   bloom     glow around the brightest parts (lamps, lit windows, the sun)
//   exposure  automatic, like the eye adapting to the light (average luminance of the image)
// and finally the tone mapping (HDR -> screen) in screen.frag.
#pragma once

#include "App.hpp"

extern bool bloomEnabled, godRaysEnabled, ssaoEnabled, lightBeamsEnabled, autoExposureEnabled;
extern gps::Shader bloomShader, godRaysShader, ssaoShader, volumetricShader, exposureShader;

const float BLOOM_THRESHOLD = 1.0f;
const float BLOOM_STRENGTH = 0.6f;
const int BLOOM_BLUR_PASSES = 5;          // each = one horizontal + one vertical blur
const float EXPOSURE = 0.8f;             // fixed exposure (auto exposure off)
const float EXPOSURE_KEY = 0.18f;        // auto exposure: the average luminance the image is brought to
const float MIN_EXPOSURE = 0.5f;         // limits: the night stays dark, the sun doesn't black the town out
const float MAX_EXPOSURE = 1.6f;
const int LIGHT_BEAM_STEPS = 48;
const float LIGHT_BEAM_DISTANCE = 200.0f;   // meters marched along each view ray
const int GOD_RAY_SAMPLES = 64;           // steps from each pixel towards the sun
const float GOD_RAYS_STRENGTH = 0.4f;
const int SSAO_SAMPLES = 16;
const float SSAO_RADIUS = 1.5f;           // meters around each point
const float SSAO_STRENGTH = 0.85f;        // 0 = no darkening, 1 = full

void initPostProcess();                               // after initShaders()
void createPostProcessBuffers(int width, int height); // with the scene framebuffers (at every resize)
void deletePostProcessBuffers();
void renderPostProcess();   // after the resolve: SSAO, god rays, light beams, bloom, exposure (into their own textures)
void renderScreenQuad();    // the scene * SSAO + light beams + bloom + god rays, tone mapped, over the whole window
void resetExposure();       // the next frame takes its exposure at once (no adaptation)
