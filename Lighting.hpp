// Lighting, time of day and fog. The sun moves along its daily path; at night a dim moon, the street lamps (point
// lights) and the floodlights on the Administrative Palace (spot lights) light the scene. The fog has the color of
// the sky. initUniforms() sets up the uniforms of the main shader (camera matrices, lights, fog); updateDayCycle()
// moves the sun every frame; applyLighting() / applyFog() update the uniforms when a setting changes.
#pragma once

#include "App.hpp"

// light parameters
extern glm::vec3 lightDir;
extern glm::vec3 lightColor;
extern GLint skyLightLoc;

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
const glm::vec3 SKY_LIGHT_COLOR(0.16f, 0.18f, 0.22f);   // ambient light of a bright sky, a little blue
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

// background color behind the skybox (the fog takes its color from the skybox, see Skybox.hpp)
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
