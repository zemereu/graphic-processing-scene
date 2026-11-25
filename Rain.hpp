// Rain and snow: line segments (drops) / points (flakes) animated entirely in the vertex shader (shaders/rain.vert),
// blown by the wind; the ground gets wet in the rain (darker, glossy, puddles) or white in the snow (basic.frag).
#pragma once

#include "App.hpp"

extern bool rainEnabled;
extern bool snowEnabled;      // snow instead of rain (one or the other)
extern float windSpeed;       // meters per second
extern float windDirection;   // degrees, 0 = blowing towards +X
extern gps::Shader rainShader;

// wet ground: grows while it rains (fully wet after WETTING_TIME), dries after it stops (DRYING_TIME)
extern float wetness;   // 0 = dry, 1 = soaked
const float WETTING_TIME = 20.0f;   // seconds
const float DRYING_TIME = 60.0f;

// snow cover: grows while it snows (first in patches, then everywhere facing the sky), melts after it stops
extern float snowCover;   // 0 = none, 1 = everything white
const float SNOW_COVER_TIME = 40.0f;   // seconds
const float SNOW_MELT_TIME = 90.0f;
const float SNOW_FALL_SPEED = 1.5f;    // meters per second

// lightning while it rains: every few seconds a flash (a few quick flickers) lights the whole scene from a random
// direction in the sky, and the sky (and so the fog) lights up around it
extern bool lightningEnabled;
extern glm::vec3 flashDirection;   // towards the flash (world space)
extern glm::vec3 flashColor;       // 0 between flashes
const float LIGHTNING_MIN_DELAY = 4.0f;    // seconds between strikes
const float LIGHTNING_MAX_DELAY = 12.0f;
const glm::vec3 LIGHTNING_COLOR(2.4f, 2.6f, 3.2f);   // cold white, HDR (the bloom makes it glow)

void initRain();
void updateWeather();   // every frame: wetness, snow cover, lightning
void setRain(bool on);  // rain and snow exclude each other
void setSnow(bool on);
void renderRain();      // rain or snow
