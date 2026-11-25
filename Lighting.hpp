// Light: time of day (sun / moon, street lamps, floodlights, fog), the sky around the scene and the
// shadow maps.
#pragma once

#include "App.hpp"

// ==============================================================================================================
// Lighting
//
// Lighting, time of day and fog. The sun moves along its daily path; at night a dim moon, the street lamps (point
// lights) and the floodlights on the Administrative Palace (spot lights) light the scene. The fog has the color of
// the sky. initUniforms() sets up the uniforms of the main shader (camera matrices, lights, fog); updateDayCycle()
// moves the sun every frame; applyLighting() / applyFog() update the uniforms when a setting changes.
// ==============================================================================================================

// light parameters
extern glm::vec3 lightDir;
extern glm::vec3 lightColor;

// time of day: the sun's path at the equinox, at latitude 45 degrees: it rises in the east (-Z) at 6:00, is highest
// in the south (+X, 45 degrees up) at 12:00 and sets in the west (+Z) at 18:00; the moon is opposite the sun
extern float timeOfDay;        // hours, 0..24
extern bool dayCycleRunning;   // the time advances on its own
extern float dayCycleSpeed;    // hours per second
const float DAY_TIME = 16.0f;  // N jumps between these two
const float NIGHT_TIME = 22.0f;
const float LATITUDE = 45.0f;  // degrees: how high the sun gets at noon (90 - latitude)
extern float dayFactor;         // 1 = day sky, 0 = night sky, in between: twilight
extern float daySkyAngle, nightSkyAngle;   // the skies turn so the glow of the sun / moon in them follows the light

// the lights of the town come on one group after the other as it gets dark (and go off in the morning):
// first the windows, then the street lamps and the cars' lights (nightMode), last the floodlights
extern bool windowsLit, nightMode, floodlightsOn;
const glm::vec3 SUN_COLOR(1.0f, 0.97f, 0.92f);
const glm::vec3 SUNSET_COLOR(1.0f, 0.5f, 0.25f);   // near the horizon the light crosses more air: red / orange
const glm::vec3 MOON_COLOR(0.10f, 0.12f, 0.20f);
const glm::vec3 LAMP_COLOR(2.0f, 1.5f, 1.0f);   // warm; above 1 so it still lights the plaza after attenuation
const glm::vec3 LAMP_GLOW(1.0f, 0.85f, 0.6f);   // color of the bulbs when they are on
const float LAMP_LIGHT_HEIGHT = 7.3f;           // light source = center of the bulb
extern std::vector<glm::vec3> LAMP_POSITIONS;   // base of each lamp post (max 16, see basic.frag)
// floodlights (spot lights) on the Administrative Palace, on at night
struct SpotLight {
    glm::vec3 position;
    glm::vec3 target;   // point the floodlight is aimed at
};
extern std::vector<SpotLight> SPOT_LIGHTS;   // max 4, see basic.frag
const glm::vec3 SPOT_COLOR(5.0f, 5.0f, 5.5f);  // cool white, strong enough to reach the top of the tower
const float SPOT_INNER_ANGLE = 10.0f;          // degrees: full intensity inside
const float SPOT_OUTER_ANGLE = 15.0f;          // degrees: fades out between inner and outer

// background color behind the skybox (the fog takes its color from the skybox, see the Skybox section)
const glm::vec3 DAY_SKY(0.7f, 0.7f, 0.7f);
const glm::vec3 NIGHT_SKY(0.02f, 0.03f, 0.06f);
extern bool fogEnabled;
extern float fogDensity;

void initUniforms();
glm::vec3 sunDirection(float hours);   // direction towards the sun (world space)
void updateDayCycle();                 // every frame: moves the sun, sets the light and which lights are on
void toggleDayNight();                 // N: jumps to DAY_TIME / NIGHT_TIME
void applyLighting();
void applyFog();

// ==============================================================================================================
// Skybox
//
// Skybox: cube maps drawn around the camera, behind everything: a sunset sky by day, a starry sky by night
// (Poly Haven "Qwantani Sunset / Night (Pure Sky)", CC0), blended during twilight and turned so the glow of the
// sun / moon in them follows the light (Lighting: dayFactor, daySkyAngle, nightSkyAngle). The same skies also give
// the color of the fog (the sky at the horizon, in the direction the camera looks) and what the water reflects.
// ==============================================================================================================

extern gps::Shader skyboxShader;
extern GLuint daySkyTexture, nightSkyTexture;
const int DAY_SKY_TEXTURE_UNIT = 6;        // units 0-2: the models' textures, 5: the shadow map
const int NIGHT_SKY_TEXTURE_UNIT = 7;
const float NIGHT_SKY_BRIGHTNESS = 0.14f;  // the night photo is exposed for the stars: much darker in the scene

void initSkybox();
void bindSkyTexture();   // the skies, their blend and rotation, for the main shader (fog, water)
void renderSkybox();     // after the opaque objects: only the pixels they left empty are drawn

// ==============================================================================================================
// Shadows
//
// Shadow mapping (Lab 9) for the directional light (sun / moon): the scene is first drawn from the light into depth
// textures; a fragment is in shadow if something closer to the light covers it there.
// Cascaded shadow maps: one map over the whole view would give blurry, blocky shadows near the camera, so the view is
// cut by distance into SHADOW_CASCADES slices and each slice gets its own map: a small, sharp one near the camera,
// bigger ones further away (the further, the less detail is needed - a texel covers fewer pixels there).
// ==============================================================================================================

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
