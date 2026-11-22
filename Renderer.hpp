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

const float LOD_DISTANCE = 250.0f;   // meters from the camera to the nearest point of the model

void initOpenGLState();
void initShaders();
void applyViewMode();
void applyShadingMode();
void createSceneFramebuffers(int width, int height);   // at the window size; recreated on every resize
void deleteSceneFramebuffers();
void renderSceneObjects();   // all the objects, used by both passes (shadow map and camera)
void renderScene();          // the camera pass
void renderScreenQuad();     // the resolved image over the whole window
