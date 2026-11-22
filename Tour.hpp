// Camera tour: the camera follows a smooth path (Catmull-Rom spline) through key points, at a roughly constant speed.
#pragma once

#include "App.hpp"

extern bool tourActive;
extern float tourTime;
extern std::vector<float> tourKeyTimes;   // time at which the camera reaches each key point

void initTour();
void startTour();
void stopTour();
void updateTour();
