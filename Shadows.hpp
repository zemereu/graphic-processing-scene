// Shadow mapping (directional light): the scene is first drawn from the sun into a depth texture;
// a fragment is in shadow if something closer to the sun covers it in that texture.
#pragma once

#include "App.hpp"

const int SHADOW_MAP_SIZE = 4096;
const float SHADOW_AREA = 800.0f;        // meters covered by the shadow map, centered on the camera
const float SHADOW_DEPTH_RANGE = 800.0f; // meters in front of / behind the center, along the sun's direction
extern bool shadowsEnabled;
extern GLuint depthMapFBO, depthMapTexture;
extern gps::Shader depthMapShader;
extern GLint depthModelLoc, depthLightSpaceLoc;    // in depthMapShader
extern glm::mat4 lightSpaceTrMatrix;
extern bool depthPass;                  // true while drawing into the shadow map
const int SHADOW_MAP_TEXTURE_UNIT = 5;   // units 0-2 are used by the models' own textures

void initShadowMap();
glm::mat4 computeLightSpaceTrMatrix();
void renderShadowMap();   // pass 1: the scene from the sun, depth only
