// Drawing the scene: the view modes, the multisampled framebuffer (antialiasing), frustum culling and level of detail,
// the list of objects drawn every frame (sorted near to far) and the final copy of the image to the window.
#pragma once

#include "App.hpp"

// view modes
enum ViewMode { VIEW_SOLID = 0, VIEW_WIREFRAME = 1, VIEW_POINTS = 2 };
extern int viewMode;
extern bool flatShading;

// antialiasing: the scene is drawn into a multisampled framebuffer of our own, resolved into a texture
// and copied to the screen (see Renderer.cpp)
extern bool antialiasingEnabled;
extern GLuint msaaFBO, msaaColorRBO, msaaDepthRBO;   // multisampled color + depth
extern GLuint resolveFBO, resolveTexture;            // single-sampled result, read by the screen pass
extern int sceneWidth, sceneHeight;
extern gps::Shader screenShader;
extern GLuint screenVAO;                             // empty: the fullscreen triangle comes from gl_VertexID

// HDR: the scene keeps colors above 1 (lamps, the sun); the screen pass adds the bloom (glow around the brightest
// parts: bright pass + Gaussian blur at half resolution) and maps everything to the screen with a filmic curve
extern bool bloomEnabled;
extern gps::Shader bloomShader;
extern GLuint bloomFBO[2], bloomTexture[2];          // half resolution, ping-pong for the blur
const float BLOOM_THRESHOLD = 1.0f;
const float BLOOM_STRENGTH = 0.6f;
const int BLOOM_BLUR_PASSES = 5;                     // each = one horizontal + one vertical blur
const float EXPOSURE = 0.8f;

const float LOD_DISTANCE = 250.0f;   // meters from the camera to the nearest point of the model

void initOpenGLState();
void initShaders();
void applyViewMode();
void applyShadingMode();
void createSceneFramebuffers(int width, int height);   // at the window size; recreated on every resize
void deleteSceneFramebuffers();
void renderSceneObjects();   // all the objects, used by both passes (shadow map and camera)
void renderScene();          // the camera pass
void renderBloom();          // after the resolve: the glow of the bright parts, into bloomTexture[0]
void renderScreenQuad();     // the resolved image (+ bloom, tone mapped) over the whole window
