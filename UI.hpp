// The control panel (Dear ImGui): FPS, view mode, lighting, fog, weather, traffic, rendering options, camera tour,
// the keyboard-controlled building and the key map.
#pragma once

#include "App.hpp"

extern bool showUI;

void initUI();       // after setWindowCallbacks(): ImGui chains to our callbacks
void renderUI();
void shutdownUI();
