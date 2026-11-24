// Shadow mapping (directional light): the scene is first drawn from the sun into a depth texture;
// a fragment is in shadow if something closer to the sun covers it in that texture.
#pragma once

#include "App.hpp"

const int SHADOW_MAP_SIZE = 4096;
// the shadows cover a box around the camera: SHADOW_AREA x SHADOW_AREA meters, from the river bed to above the towers
const float SHADOW_AREA = 800.0f;
const float SHADOW_MIN_Y = -10.0f;
const float SHADOW_MAX_Y = 200.0f;
const float SHADOW_CASTER_RANGE = 1500.0f;  // meters towards the light beyond the box: what stands there still casts
                                            // shadows into it (the mountains, a tall building when the sun is low)
extern bool shadowsEnabled;
extern bool showShadowMap;              // preview of the depth map in the UI
extern GLuint depthMapFBO, depthMapTexture;
extern gps::Shader depthMapShader;
extern GLint depthModelLoc, depthLightSpaceLoc;    // in depthMapShader
extern glm::mat4 lightSpaceTrMatrix;
extern bool depthPass;                  // true while drawing into a shadow map
extern glm::mat4 depthPassMatrix;       // that shadow map's light space matrix (culling)
extern bool spotShadowPass;             // true while drawing a floodlight's shadow map
const int SHADOW_MAP_TEXTURE_UNIT = 5;   // units 0-2 are used by the models' own textures

// floodlight shadows: every spot light gets its own shadow map, with a perspective projection (the light spreads from
// a point, in a cone); only while the floodlights are on. One 2D array texture, one layer per floodlight.
const int SPOT_SHADOW_MAP_SIZE = 1024;
const int SPOT_SHADOW_TEXTURE_UNIT = 8;
const float SPOT_SHADOW_NEAR = 0.5f;     // meters
const float SPOT_SHADOW_FAR = 250.0f;
extern GLuint spotShadowFBO, spotShadowTexture;
extern glm::mat4 spotLightSpaceMatrices[4];   // max 4 spot lights, see basic.frag

void initShadowMap();
glm::mat4 computeLightSpaceTrMatrix();
void renderShadowMap();       // pass 1: the scene from the sun, depth only
void renderSpotShadowMaps();  // pass 1b: the scene from each floodlight (at night)
void bindShadowMaps();        // the shadow maps and their matrices, for the main shader
