// Drawing a frame: view modes, the antialiased framebuffer, culling, the draw list, the order of the
// passes, and the water reflections.
#pragma once

#include "App.hpp"

// ==============================================================================================================
// Renderer
//
// Drawing the scene: the view modes, the multisampled framebuffer (antialiasing), frustum culling and level of detail,
// the list of objects drawn every frame (sorted near to far) and the order of the passes of a frame.
// ==============================================================================================================

// view modes
enum ViewMode { VIEW_SOLID = 0, VIEW_WIREFRAME = 1, VIEW_POINTS = 2 };
extern int viewMode;
extern bool flatShading;

// antialiasing: the scene is drawn into a multisampled framebuffer of our own, resolved into a texture
// and copied to the screen (see Renderer.cpp)
extern bool antialiasingEnabled;
extern GLuint msaaFBO, msaaColorRBO, msaaDepthRBO;   // multisampled color + depth
extern GLuint resolveFBO, resolveTexture;            // single-sampled result, read by the screen pass
extern GLuint resolveDepthTexture;                   // its depth, for the post-processing (SSAO, god rays)
extern int sceneWidth, sceneHeight;
extern gps::Shader screenShader;
extern GLuint screenVAO;                             // empty: the fullscreen triangle comes from gl_VertexID

const float LOD_DISTANCE = 250.0f;   // meters from the camera to the nearest point of the model

// graphics quality: presets that turn off the expensive effects and draw less far away (UI / key G)
enum GraphicsQuality { QUALITY_LOW = 0, QUALITY_MEDIUM = 1, QUALITY_HIGH = 2 };
extern int graphicsQuality;
extern float drawDistanceScale;   // the vehicles, the details and the high-detail models: their distances times this
void applyGraphicsQuality();

void initOpenGLState();
void initShaders();
void applyViewMode();
void applyShadingMode();
void createSceneFramebuffers(int width, int height);   // at the window size; recreated on every resize
void deleteSceneFramebuffers();
void renderSceneObjects();   // all the objects, used by both passes (shadow map and camera)
void renderScene();          // the camera pass
void renderFrame();          // the whole frame: shadow maps, scene, resolve, post-processing, to the window

// ==============================================================================================================
// Reflection
//
// Water reflections (planar reflection): before the normal pass, the scene is drawn a second time as seen in a mirror
// laid on the water surface (the view is mirrored across the plane y = WATER_LEVEL; whatever is under the water is
// cut off with a clip plane) into a half-resolution texture. The water shader reads it at its own pixel on the
// screen, shifted a little by the waves.
// ==============================================================================================================

extern bool reflectionsEnabled;
extern bool reflectionPass;   // true while drawing the mirrored scene
const float WATER_LEVEL = -2.0f;
const int REFLECTION_TEXTURE_UNIT = 9;

void initReflection();                                // after initShaders()
void createReflectionBuffers(int width, int height);  // with the scene framebuffers
void deleteReflectionBuffers();
void renderReflection();   // the mirrored scene, into the reflection texture
void bindReflection();     // the reflection texture, for the main shader (water)
