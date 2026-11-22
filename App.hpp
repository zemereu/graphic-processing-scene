// Shared state of the program: the window, the camera with its view / projection matrices, the frame time,
// the keys held down and the main shader (basic.vert / basic.frag) with the locations of its uniforms.
// Every other module includes this header.
#pragma once

#include "Window.h"
#include "Shader.hpp"
#include "Camera.hpp"
#include "Model3D.hpp"

#include <glm/glm.hpp> //core glm functionality
#include <glm/gtc/matrix_transform.hpp> //glm extension for generating common transformation matrices
#include <glm/gtc/matrix_inverse.hpp> //glm extension for computing inverse matrices
#include <glm/gtc/type_ptr.hpp> //glm extension for accessing the internal data structure of glm types
#include <glm/gtc/constants.hpp> //glm::two_pi

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

// window
extern gps::Window myWindow;

// matrices
extern glm::mat4 view;
extern glm::mat4 projection;

// camera
extern gps::Camera myCamera;

// time
extern double lastFrameTime;
extern float deltaTime;   // seconds since the previous frame

// keyboard: true while the key is held down
extern GLboolean pressedKeys[1024];

// the main shader and the locations of its uniforms
extern gps::Shader myBasicShader;
extern GLint modelLoc;
extern GLint viewLoc;
extern GLint projectionLoc;
extern GLint normalMatrixLoc;
extern GLint lightDirLoc;
extern GLint lightColorLoc;
extern GLint numPointLightsLoc;
extern GLint pointLightPosLoc;
extern GLint pointLightColorLoc;
extern GLint emissiveColorLoc;
extern GLint surfaceModeLoc, timeLoc;  // terrain / water shading modes (basic.frag), time for the waves
extern GLint numSpotLightsLoc;
extern GLint spotLightPosLoc;
extern GLint spotLightDirLoc;
extern GLint spotLightColorLoc;
extern GLint spotCutOffLoc;
extern GLint spotOuterCutOffLoc;
extern GLint flatShadingLoc;
extern GLint fogDensityLoc;
extern GLint fogColorLoc;
extern GLint lightSpaceTrMatrixLoc, shadowMapLoc, shadowsEnabledLoc;   // shadow mapping

GLenum glCheckError_(const char* file, int line);
#define glCheckError() glCheckError_(__FILE__, __LINE__)

// sends the camera's view matrix to the main shader
void uploadView();
