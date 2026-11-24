// Rain: line segments animated entirely in the vertex shader (shaders/rain.vert), blown by the wind; the ground gets
// wet (darker, glossy, puddles; basic.frag).
#pragma once

#include "App.hpp"

extern bool rainEnabled;
extern float windSpeed;       // meters per second
extern float windDirection;   // degrees, 0 = blowing towards +X
extern gps::Shader rainShader;

// wet ground: grows while it rains (fully wet after WETTING_TIME), dries after it stops (DRYING_TIME)
extern float wetness;   // 0 = dry, 1 = soaked
const float WETTING_TIME = 20.0f;   // seconds
const float DRYING_TIME = 60.0f;

// lightning while it rains: every few seconds a flash (a few quick flickers) lights the whole scene from a random
// direction in the sky, and the sky (and so the fog) lights up around it
extern bool lightningEnabled;
extern glm::vec3 flashDirection;   // towards the flash (world space)
extern glm::vec3 flashColor;       // 0 between flashes
const float LIGHTNING_MIN_DELAY = 4.0f;    // seconds between strikes
const float LIGHTNING_MAX_DELAY = 12.0f;
const glm::vec3 LIGHTNING_COLOR(2.4f, 2.6f, 3.2f);   // cold white, HDR (the bloom makes it glow)

void initRain();
void updateWeather();   // every frame: wetness, lightning
void renderRain();
