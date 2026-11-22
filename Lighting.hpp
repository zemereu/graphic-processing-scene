// Lighting and fog. Day: the sun only. Night: a dim moon, the street lamps (point lights) and the floodlights on the
// Administrative Palace (spot lights). The fog has the color of the sky. initUniforms() sets up the uniforms of the
// main shader (camera matrices, lights, fog); applyLighting() / applyFog() update them when a setting changes.
#pragma once

#include "App.hpp"

// light parameters
extern glm::vec3 lightDir;
extern glm::vec3 lightColor;

// lighting: day = sun only, night = dim moon + street lamps (point lights)
extern bool nightMode;
const glm::vec3 SUN_COLOR(1.0f, 1.0f, 1.0f);
const glm::vec3 MOON_COLOR(0.10f, 0.12f, 0.20f);
const glm::vec3 LAMP_COLOR(2.0f, 1.5f, 1.0f);   // warm; above 1 so it still lights the plaza after attenuation
const glm::vec3 LAMP_GLOW(1.0f, 0.85f, 0.6f);   // color of the bulbs when they are on
const float LAMP_LIGHT_HEIGHT = 7.3f;           // light source = center of the bulb
extern const std::vector<glm::vec3> LAMP_POSITIONS;   // base of each lamp post (max 16, see basic.frag)
// floodlights (spot lights) on the Administrative Palace, on at night
struct SpotLight {
    glm::vec3 position;
    glm::vec3 target;   // point the floodlight is aimed at
};
extern const std::vector<SpotLight> SPOT_LIGHTS;   // max 4, see basic.frag
const glm::vec3 SPOT_COLOR(5.0f, 5.0f, 5.5f);  // cool white, strong enough to reach the top of the tower
const float SPOT_INNER_ANGLE = 10.0f;          // degrees: full intensity inside
const float SPOT_OUTER_ANGLE = 15.0f;          // degrees: fades out between inner and outer

// fog: distant objects fade into the sky color
const glm::vec3 DAY_SKY(0.7f, 0.7f, 0.7f);
const glm::vec3 NIGHT_SKY(0.02f, 0.03f, 0.06f);
extern bool fogEnabled;
extern float fogDensity;

void initUniforms();
void applyLighting();
void applyFog();
