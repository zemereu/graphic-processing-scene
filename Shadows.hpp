// Shadow mapping (Lab 9) for the directional light (sun / moon): the scene is first drawn from the light into depth
// textures; a fragment is in shadow if something closer to the light covers it there.
// Cascaded shadow maps: one map over the whole view would give blurry, blocky shadows near the camera, so the view is
// cut by distance into SHADOW_CASCADES slices and each slice gets its own map: a small, sharp one near the camera,
// bigger ones further away (the further, the less detail is needed - a texel covers fewer pixels there).
#pragma once

#include "App.hpp"

const int SHADOW_CASCADES = 3;
const int SHADOW_MAP_SIZE = 2048;   // per cascade
// where each cascade ends (meters from the camera, along the view direction); the first starts at the near plane
const float CASCADE_SPLITS[SHADOW_CASCADES + 1] = { 0.5f, 30.0f, 130.0f, 600.0f };
const float SHADOW_CASTER_RANGE = 1500.0f;  // meters towards the light beyond a cascade: what stands there still casts
                                            // shadows into it (the mountains, a tall building when the sun is low)
extern bool shadowsEnabled;
extern bool showShadowMap;              // preview of the cascades in the UI
extern GLuint depthMapFBO, depthMapTexture;   // depth texture array, one layer per cascade
extern gps::Shader depthMapShader;
extern GLint depthModelLoc, depthLightSpaceLoc;    // in depthMapShader
extern glm::mat4 lightSpaceMatrices[SHADOW_CASCADES];
extern GLuint shadowPreviewTexture[SHADOW_CASCADES];   // the cascades in grayscale, for the UI
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
void computeLightSpaceMatrices();   // the orthographic projection of each cascade
void renderShadowMap();       // pass 1: the scene from the sun / moon, depth only, once per cascade
void renderSpotShadowMaps();  // pass 1b: the scene from each floodlight (at night)
void bindShadowMaps();        // the shadow maps and their matrices, for the main shader
void renderShadowPreview();   // the cascades into shadowPreviewTexture (only while the preview is shown)
