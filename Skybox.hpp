// Skybox: cube maps drawn around the camera, behind everything: a sunset sky by day, a starry sky by night
// (Poly Haven "Qwantani Sunset / Night (Pure Sky)", CC0), blended during twilight and turned so the glow of the
// sun / moon in them follows the light (Lighting: dayFactor, daySkyAngle, nightSkyAngle). The same skies also give
// the color of the fog (the sky at the horizon, in the direction the camera looks) and what the water reflects.
#pragma once

#include "App.hpp"

extern gps::Shader skyboxShader;
extern GLuint daySkyTexture, nightSkyTexture;
const int DAY_SKY_TEXTURE_UNIT = 6;        // units 0-2: the models' textures, 5: the shadow map
const int NIGHT_SKY_TEXTURE_UNIT = 7;
const float NIGHT_SKY_BRIGHTNESS = 0.14f;  // the night photo is exposed for the stars: much darker in the scene

void initSkybox();
void bindSkyTexture();   // the skies, their blend and rotation, for the main shader (fog, water)
void renderSkybox();     // after the opaque objects: only the pixels they left empty are drawn
