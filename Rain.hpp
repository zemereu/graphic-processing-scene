// Rain: line segments animated entirely in the vertex shader (shaders/rain.vert), blown by the wind.
#pragma once

#include "App.hpp"

extern bool rainEnabled;
extern float windSpeed;       // meters per second
extern float windDirection;   // degrees, 0 = blowing towards +X
extern gps::Shader rainShader;

void initRain();
void renderRain();
