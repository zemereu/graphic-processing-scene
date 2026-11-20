#if defined (__APPLE__)
#define GLFW_INCLUDE_GLCOREARB
#define GL_SILENCE_DEPRECATION
#else
#define GLEW_STATIC
#include <GL/glew.h>
#endif

#include <GLFW/glfw3.h>

#include <glm/glm.hpp> //core glm functionality
#include <glm/gtc/matrix_transform.hpp> //glm extension for generating common transformation matrices
#include <glm/gtc/matrix_inverse.hpp> //glm extension for computing inverse matrices
#include <glm/gtc/type_ptr.hpp> //glm extension for accessing the internal data structure of glm types

#include "Window.h"
#include "Shader.hpp"
#include "Camera.hpp"
#include "Model3D.hpp"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#include <iostream>
#include <cmath>
#include <vector>
#include <random>
#include <cfloat>

#ifdef _WIN32
// on laptops with two GPUs, ask the driver to run this program on the dedicated one
extern "C" {
    __declspec(dllexport) unsigned long NvOptimusEnablement = 1;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

// window
gps::Window myWindow;

// matrices
glm::mat4 view;
glm::mat4 projection;

// light parameters
glm::vec3 lightDir;
glm::vec3 lightColor;

// shader uniform locations
GLint modelLoc;
GLint viewLoc;
GLint projectionLoc;
GLint normalMatrixLoc;
GLint lightDirLoc;
GLint lightColorLoc;
GLint numPointLightsLoc;
GLint pointLightPosLoc;
GLint pointLightColorLoc;
GLint emissiveColorLoc;
GLint numSpotLightsLoc;
GLint spotLightPosLoc;
GLint spotLightDirLoc;
GLint spotLightColorLoc;
GLint spotCutOffLoc;
GLint spotOuterCutOffLoc;

// camera
gps::Camera myCamera(
    glm::vec3(40.0f, 35.0f, 380.0f),
    glm::vec3(40.0f, 35.0f, 0.0f),
    glm::vec3(0.0f, 1.0f, 0.0f));

float cameraSpeed = 30.0f;
float cameraSprintFactor = 4.0f;

// time
double lastFrameTime = 0.0;
float deltaTime = 0.0f;

GLboolean pressedKeys[1024];

// mouse
bool firstMouse = true;
double lastX, lastY;
float yaw = -90.0f, pitch = 0.0f;
float mouseSensitivity = 0.1f;

// models 
gps::Model3D terrain;
gps::Model3D satuMare;
gps::Model3D genex;
gps::Model3D genexRestaurant;
gps::Model3D panorama;
gps::Model3D telefoane;

// animations
float restaurantAngle = 0.0f;
const float RESTAURANT_SPEED = 6.0f;

// keyboard-controlled object (Palatul Telefoanelor)
const glm::vec3 TELEFOANE_START_POS(-170.0f, 0.0f, 140.0f);
glm::vec3 telefoanePos = TELEFOANE_START_POS;
float telefoaneAngle = 0.0f;   // degrees, around its own vertical axis
float telefoaneScale = 1.0f;   // uniform scale

const float OBJECT_MOVE_SPEED = 20.0f;    // meters per second
const float OBJECT_ROTATE_SPEED = 45.0f;  // degrees per second
const float OBJECT_SCALE_SPEED = 0.5f;    // +50% per second

// view modes
enum ViewMode { VIEW_SOLID = 0, VIEW_WIREFRAME = 1, VIEW_POINTS = 2 };
int viewMode = VIEW_SOLID;
bool flatShading = false;
GLint flatShadingLoc;

// lighting: day = sun only, night = dim moon + street lamps (point lights)
bool nightMode = false;
const glm::vec3 SUN_COLOR(1.0f, 1.0f, 1.0f);
const glm::vec3 MOON_COLOR(0.10f, 0.12f, 0.20f);
const glm::vec3 LAMP_COLOR(2.0f, 1.5f, 1.0f);   // warm; above 1 so it still lights the plaza after attenuation
const glm::vec3 LAMP_GLOW(1.0f, 0.85f, 0.6f);   // color of the bulbs when they are on
const float LAMP_LIGHT_HEIGHT = 7.3f;           // light source = center of the bulb
const std::vector<glm::vec3> LAMP_POSITIONS = { // base of each lamp post (max 16, see basic.frag)
    // plaza in front of the Administrative Palace
    {-60.0f, 0.0f, 130.0f}, {-20.0f, 0.0f, 130.0f}, {20.0f, 0.0f, 130.0f}, {60.0f, 0.0f, 130.0f},
    {-60.0f, 0.0f, 170.0f}, {-20.0f, 0.0f, 170.0f}, {20.0f, 0.0f, 170.0f}, {60.0f, 0.0f, 170.0f},
    // plaza of Palatul Telefoanelor
    {-195.0f, 0.0f, 175.0f}, {-145.0f, 0.0f, 175.0f},
    // road under Genex
    {225.0f, 0.0f, 186.0f}, {295.0f, 0.0f, 186.0f},
};
// floodlights (spot lights) on the Administrative Palace, on at night
struct SpotLight {
    glm::vec3 position;
    glm::vec3 target;   // point the floodlight is aimed at
};
const std::vector<SpotLight> SPOT_LIGHTS = { // max 4, see basic.frag
    {{-5.0f, 14.5f, 84.0f}, {0.0f, 70.0f, 68.0f}},  // on the roof of the low wing, aimed at the main tower
    {{5.0f, 14.5f, 84.0f},  {0.0f, 70.0f, 68.0f}},
    {{25.0f, 0.2f, 90.0f},  {19.0f, 40.0f, 72.0f}}, // on the plaza, aimed at the second block
};
const glm::vec3 SPOT_COLOR(5.0f, 5.0f, 5.5f);  // cool white, strong enough to reach the top of the tower
const float SPOT_INNER_ANGLE = 10.0f;          // degrees: full intensity inside
const float SPOT_OUTER_ANGLE = 15.0f;          // degrees: fades out between inner and outer
gps::Model3D floodlight;
gps::Model3D lampPost;
gps::Model3D lampBulb;

// camera tour: the camera follows a smooth path through these key points
struct TourKey {
    glm::vec3 position;
    glm::vec3 target;   // point the camera looks at
};
const std::vector<TourKey> TOUR = {
    {{40.0f, 35.0f, 380.0f},  {0.0f, 30.0f, 60.0f}},      // overview from the south
    {{0.0f, 12.0f, 240.0f},   {0.0f, 20.0f, 60.0f}},      // approach the plaza
    {{-30.0f, 6.0f, 150.0f},  {0.0f, 60.0f, 60.0f}},      // look up at the Administrative Palace
    {{-120.0f, 15.0f, 215.0f}, {-170.0f, 10.0f, 140.0f}}, // Palatul Telefoanelor
    {{-130.0f, 25.0f, 60.0f}, {-60.0f, 50.0f, -260.0f}},  // turn north, towards the river
    {{-60.0f, 60.0f, -120.0f}, {-60.0f, 50.0f, -260.0f}}, // over the river, Hotel Panorama
    {{120.0f, 50.0f, -40.0f}, {260.0f, 60.0f, 196.0f}},   // swing east towards Genex
    {{170.0f, 8.0f, 196.0f},  {260.0f, 30.0f, 196.0f}},   // down to the road
    {{300.0f, 8.0f, 196.0f},  {360.0f, 10.0f, 196.0f}},   // through the gate, under the bridge
    {{360.0f, 40.0f, 260.0f}, {260.0f, 100.0f, 210.0f}},  // look back at the tower and the restaurant
    {{40.0f, 35.0f, 380.0f},  {0.0f, 30.0f, 60.0f}},      // back to the start
};
const float TOUR_SPEED = 25.0f;       // meters per second along the path
const float TOUR_MIN_SEGMENT = 3.0f;  // seconds, so short segments are not rushed
std::vector<float> tourKeyTimes;      // time at which the camera reaches each key point
bool tourActive = false;
float tourTime = 0.0f;

// fog: distant objects fade into the sky color
const glm::vec3 DAY_SKY(0.7f, 0.7f, 0.7f);
const glm::vec3 NIGHT_SKY(0.02f, 0.03f, 0.06f);
bool fogEnabled = true;
float fogDensity = 0.0018f;
GLint fogDensityLoc;
GLint fogColorLoc;

// rain: line segments animated entirely in the vertex shader (shaders/rain.vert)
const int RAIN_DROPS = 20000;
const float RAIN_FALL_SPEED = 25.0f;   // meters per second
const float RAIN_AREA_SIZE = 80.0f;    // the rain box around the camera, meters
const float RAIN_AREA_HEIGHT = 40.0f;
const float RAIN_DROP_LENGTH = 0.8f;
bool rainEnabled = false;
float windSpeed = 6.0f;        // meters per second
float windDirection = 30.0f;   // degrees, 0 = blowing towards +X
gps::Shader rainShader;
GLuint rainVAO, rainVBO;
GLint rainViewLoc, rainProjectionLoc, rainCameraPosLoc, rainTimeLoc, rainFallSpeedLoc, rainWindLoc;
GLint rainAreaSizeLoc, rainAreaHeightLoc, rainDropLengthLoc, rainColorLoc;

// collision detection: the camera is a sphere that can't enter the buildings' boxes
const float CAMERA_RADIUS = 1.5f;      // meters
const float CAMERA_MIN_HEIGHT = 1.8f;  // eye height above the ground plane (y = 0)
bool collisionsEnabled = true;
std::vector<gps::BoundingBox> satuMareBounds, genexBounds, restaurantBounds, panoramaBounds,
                              telefoaneBounds, lampPostBounds, floodlightBounds; // model space, from the meshes
std::vector<gps::BoundingBox> worldColliders; // world space, rebuilt every frame (some objects move)

// UI
bool showUI = true;

// shaders
gps::Shader myBasicShader;

GLenum glCheckError_(const char* file, int line) {
    GLenum errorCode;
    while ((errorCode = glGetError()) != GL_NO_ERROR) {
        std::string error;
        switch (errorCode) {
        case GL_INVALID_ENUM:
            error = "INVALID_ENUM";
            break;
        case GL_INVALID_VALUE:
            error = "INVALID_VALUE";
            break;
        case GL_INVALID_OPERATION:
            error = "INVALID_OPERATION";
            break;
        case GL_OUT_OF_MEMORY:
            error = "OUT_OF_MEMORY";
            break;
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            error = "INVALID_FRAMEBUFFER_OPERATION";
            break;
        }
        std::cout << error << " | " << file << " (" << line << ")" << std::endl;
    }
    return errorCode;
}
#define glCheckError() glCheckError_(__FILE__, __LINE__)

void windowResizeCallback(GLFWwindow* window, int width, int height) {
    fprintf(stdout, "Window resized! New width: %d , and height: %d\n", width, height);
    if (width == 0 || height == 0) return; // window minimized
    firstMouse = true; // the cursor is re-centered on resize; don't treat that as mouse movement

    int fbWidth, fbHeight;
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    myWindow.setWindowDimensions(WindowDimensions{ fbWidth, fbHeight });
    glViewport(0, 0, fbWidth, fbHeight);

    projection = glm::perspective(glm::radians(45.0f), (float)fbWidth / (float)fbHeight, 0.5f, 1500.0f);
    myBasicShader.useShaderProgram();
    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));
}

void applyViewMode() {
    GLenum mode = GL_FILL;
    if (viewMode == VIEW_WIREFRAME) mode = GL_LINE;
    if (viewMode == VIEW_POINTS) mode = GL_POINT;
    glPolygonMode(GL_FRONT_AND_BACK, mode);
}

void applyShadingMode() {
    myBasicShader.useShaderProgram();
    glUniform1i(flatShadingLoc, flatShading);
}

// model matrices, shared by rendering and collision detection
glm::mat4 satuMareMatrix() { return glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 60.0f)); }
glm::mat4 genexMatrix() { return glm::translate(glm::mat4(1.0f), glm::vec3(260.0f, 0.0f, 196.0f)); }
glm::mat4 panoramaMatrix() { return glm::translate(glm::mat4(1.0f), glm::vec3(-60.0f, 40.0f, -260.0f)); }

glm::mat4 restaurantMatrix() {
    // rotate around its own axis first, then move it on top of the tower (T * R)
    glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(260.0f, 108.0f, 229.0f));
    return glm::rotate(m, glm::radians(restaurantAngle), glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 telefoaneMatrix() {
    // T * R * S: scale and rotate around the building's own base, then move it into place
    glm::mat4 m = glm::translate(glm::mat4(1.0f), telefoanePos);
    m = glm::rotate(m, glm::radians(telefoaneAngle), glm::vec3(0.0f, 1.0f, 0.0f));
    return glm::scale(m, glm::vec3(telefoaneScale));
}

// box in model space -> box in world space that contains it (the 8 corners are transformed)
gps::BoundingBox transformBox(const gps::BoundingBox& box, const glm::mat4& m) {
    gps::BoundingBox result = { glm::vec3(FLT_MAX), glm::vec3(-FLT_MAX) };
    for (int i = 0; i < 8; i++) {
        glm::vec3 corner((i & 1) ? box.max.x : box.min.x,
                         (i & 2) ? box.max.y : box.min.y,
                         (i & 4) ? box.max.z : box.min.z);
        glm::vec3 p = glm::vec3(m * glm::vec4(corner, 1.0f));
        result.min = glm::min(result.min, p);
        result.max = glm::max(result.max, p);
    }
    return result;
}

void addColliders(const std::vector<gps::BoundingBox>& boxes, const glm::mat4& m) {
    for (const gps::BoundingBox& box : boxes) {
        worldColliders.push_back(transformBox(box, m));
    }
}

void updateColliders() {
    worldColliders.clear();
    addColliders(satuMareBounds, satuMareMatrix());
    addColliders(genexBounds, genexMatrix());
    addColliders(restaurantBounds, restaurantMatrix());
    addColliders(panoramaBounds, panoramaMatrix());
    addColliders(telefoaneBounds, telefoaneMatrix());
    for (const glm::vec3& base : LAMP_POSITIONS) {
        addColliders(lampPostBounds, glm::translate(glm::mat4(1.0f), base));
    }
    for (const SpotLight& spot : SPOT_LIGHTS) {
        addColliders(floodlightBounds, glm::translate(glm::mat4(1.0f), spot.position));
    }
}

// sphere vs box: the closest point of the box to the sphere center must be farther than the radius
bool collides(const glm::vec3& center) {
    for (const gps::BoundingBox& box : worldColliders) {
        glm::vec3 closest = glm::clamp(center, box.min, box.max);
        glm::vec3 d = center - closest;
        if (glm::dot(d, d) < CAMERA_RADIUS * CAMERA_RADIUS) {
            return true;
        }
    }
    return false;
}

// moves the camera, but stops it at walls; the free components of the move are kept,
// so the camera slides along a wall instead of getting stuck on it
void moveCamera(gps::MOVE_DIRECTION direction, float distance) {
    glm::vec3 oldPos = myCamera.getPosition();
    myCamera.move(direction, distance);
    if (!collisionsEnabled) return;

    glm::vec3 newPos = myCamera.getPosition();
    newPos.y = glm::max(newPos.y, CAMERA_MIN_HEIGHT);
    if (!collides(newPos) || collides(oldPos)) { // already inside (e.g. collisions just turned on): let it out
        myCamera.setPosition(newPos);
        return;
    }

    glm::vec3 pos = oldPos;
    glm::vec3 delta = newPos - oldPos;
    for (int axis = 0; axis < 3; axis++) {
        glm::vec3 attempt = pos;
        attempt[axis] += delta[axis];
        if (!collides(attempt)) {
            pos = attempt;
        }
    }
    myCamera.setPosition(pos);
}

void applyLighting() {
    myBasicShader.useShaderProgram();
    lightColor = nightMode ? MOON_COLOR : SUN_COLOR;
    glUniform3fv(lightColorLoc, 1, glm::value_ptr(lightColor));
    glUniform1i(numPointLightsLoc, nightMode ? (GLint)LAMP_POSITIONS.size() : 0);
    glUniform1i(numSpotLightsLoc, nightMode ? (GLint)SPOT_LIGHTS.size() : 0);
    // the fog has the same color as the sky, so far objects blend into the background
    glm::vec3 sky = nightMode ? NIGHT_SKY : DAY_SKY;
    glClearColor(sky.r, sky.g, sky.b, 1.0f);
    glUniform3fv(fogColorLoc, 1, glm::value_ptr(sky));
}

void applyFog() {
    myBasicShader.useShaderProgram();
    glUniform1f(fogDensityLoc, fogEnabled ? fogDensity : 0.0f);
}

// Catmull-Rom spline: passes through p1 (t = 0) and p2 (t = 1), p0 and p3 shape the curve
glm::vec3 catmullRom(const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3, float t) {
    float t2 = t * t, t3 = t2 * t;
    return 0.5f * ((2.0f * p1) + (-p0 + p2) * t + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2
        + (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
}

void initTour() {
    // each segment lasts proportionally to its length -> roughly constant speed
    tourKeyTimes = { 0.0f };
    for (size_t i = 0; i + 1 < TOUR.size(); i++) {
        float length = glm::distance(TOUR[i].position, TOUR[i + 1].position);
        tourKeyTimes.push_back(tourKeyTimes.back() + glm::max(TOUR_MIN_SEGMENT, length / TOUR_SPEED));
    }
}

void uploadView() {
    view = myCamera.getViewMatrix();
    myBasicShader.useShaderProgram();
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
}

void startTour() {
    tourActive = true;
    tourTime = 0.0f;
}

void stopTour() {
    tourActive = false;
    // continue with mouse/keyboard from where the tour left the camera, without a jump
    glm::vec3 front = myCamera.getFrontDirection();
    yaw = glm::degrees(atan2(front.z, front.x));
    pitch = glm::degrees(asin(front.y));
    firstMouse = true;
}

void updateTour() {
    if (!tourActive) return;

    tourTime += deltaTime;
    if (tourTime >= tourKeyTimes.back()) {
        myCamera.setLookAt(TOUR.back().position, TOUR.back().target);
        uploadView();
        stopTour();
        return;
    }

    // current segment i: between key i and key i + 1
    int i = 0;
    while (tourKeyTimes[i + 1] < tourTime) i++;
    float t = (tourTime - tourKeyTimes[i]) / (tourKeyTimes[i + 1] - tourKeyTimes[i]);

    auto key = [](int k) { return TOUR[glm::clamp(k, 0, (int)TOUR.size() - 1)]; };
    glm::vec3 position = catmullRom(key(i - 1).position, key(i).position, key(i + 1).position, key(i + 2).position, t);
    glm::vec3 target = catmullRom(key(i - 1).target, key(i).target, key(i + 1).target, key(i + 2).target, t);

    myCamera.setLookAt(position, target);
    uploadView();
}

void resetTelefoane() {
    telefoanePos = TELEFOANE_START_POS;
    telefoaneAngle = 0.0f;
    telefoaneScale = 1.0f;
}

void keyboardCallback(GLFWwindow* window, int key, int scancode, int action, int mode) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) { glfwSetWindowShouldClose(window, GL_TRUE); }

    if (key == GLFW_KEY_M && action == GLFW_PRESS) {
        if (glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED)
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        else
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        firstMouse = true;
    }

    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_1) { viewMode = VIEW_SOLID; applyViewMode(); }
        if (key == GLFW_KEY_2) { viewMode = VIEW_WIREFRAME; applyViewMode(); }
        if (key == GLFW_KEY_3) { viewMode = VIEW_POINTS; applyViewMode(); }
        if (key == GLFW_KEY_4) { flatShading = !flatShading; applyShadingMode(); }
        if (key == GLFW_KEY_H) showUI = !showUI;
        if (key == GLFW_KEY_N) { nightMode = !nightMode; applyLighting(); }
        if (key == GLFW_KEY_F) { fogEnabled = !fogEnabled; applyFog(); }
        if (key == GLFW_KEY_P) rainEnabled = !rainEnabled;
        if (key == GLFW_KEY_C) collisionsEnabled = !collisionsEnabled;
        if (key == GLFW_KEY_T) { if (tourActive) stopTour(); else startTour(); }
    }

    if (key >= 0 && key < 1024) {
        if (action == GLFW_PRESS) {
            pressedKeys[key] = true;
        }
        else if (action == GLFW_RELEASE) {
            pressedKeys[key] = false;
        }
    }
}

void mouseCallback(GLFWwindow* window, double xpos, double ypos) {
    if (glfwGetInputMode(window, GLFW_CURSOR) != GLFW_CURSOR_DISABLED) return;
    if (tourActive) return; // the tour controls the camera

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
        return;
    }

    float xoffset = (float)(xpos - lastX) * mouseSensitivity;
    float yoffset = (float)(lastY - ypos) * mouseSensitivity;
    lastX = xpos;
    lastY = ypos;

    yaw += xoffset;
    pitch += yoffset;
    pitch = glm::clamp(pitch, -89.0f, 89.0f);

    myCamera.rotate(pitch, yaw);
    view = myCamera.getViewMatrix();
    myBasicShader.useShaderProgram();
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
}

void processMovement() {
    if (tourActive) return; // the tour controls the camera

    updateColliders(); // the building and the restaurant may have moved since the last frame

    float distance = cameraSpeed * deltaTime;
    if (pressedKeys[GLFW_KEY_LEFT_SHIFT]) {
        distance *= cameraSprintFactor;
    }

    if (pressedKeys[GLFW_KEY_W]) {
        moveCamera(gps::MOVE_FORWARD, distance);
        //update view matrix
        view = myCamera.getViewMatrix();
        myBasicShader.useShaderProgram();
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    }

    if (pressedKeys[GLFW_KEY_S]) {
        moveCamera(gps::MOVE_BACKWARD, distance);
        //update view matrix
        view = myCamera.getViewMatrix();
        myBasicShader.useShaderProgram();
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    }

    if (pressedKeys[GLFW_KEY_A]) {
        moveCamera(gps::MOVE_LEFT, distance);
        //update view matrix
        view = myCamera.getViewMatrix();
        myBasicShader.useShaderProgram();
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    }

    if (pressedKeys[GLFW_KEY_D]) {
        moveCamera(gps::MOVE_RIGHT, distance);
        //update view matrix
        view = myCamera.getViewMatrix();
        myBasicShader.useShaderProgram();
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    }
}

// arrows: move, Q/E: rotate, Z/X: scale, R: reset
void processObjectTransform() {
    if (pressedKeys[GLFW_KEY_UP])    telefoanePos.z -= OBJECT_MOVE_SPEED * deltaTime;
    if (pressedKeys[GLFW_KEY_DOWN])  telefoanePos.z += OBJECT_MOVE_SPEED * deltaTime;
    if (pressedKeys[GLFW_KEY_LEFT])  telefoanePos.x -= OBJECT_MOVE_SPEED * deltaTime;
    if (pressedKeys[GLFW_KEY_RIGHT]) telefoanePos.x += OBJECT_MOVE_SPEED * deltaTime;

    if (pressedKeys[GLFW_KEY_Q]) telefoaneAngle += OBJECT_ROTATE_SPEED * deltaTime;
    if (pressedKeys[GLFW_KEY_E]) telefoaneAngle -= OBJECT_ROTATE_SPEED * deltaTime;

    if (pressedKeys[GLFW_KEY_X]) telefoaneScale *= 1.0f + OBJECT_SCALE_SPEED * deltaTime;
    if (pressedKeys[GLFW_KEY_Z]) telefoaneScale /= 1.0f + OBJECT_SCALE_SPEED * deltaTime;
    telefoaneScale = glm::clamp(telefoaneScale, 0.2f, 3.0f);

    if (pressedKeys[GLFW_KEY_R]) resetTelefoane();
}

void updateAnimations() {
    restaurantAngle = fmod(restaurantAngle + RESTAURANT_SPEED * deltaTime, 360.0f);
}

void initOpenGLWindow() {
    myWindow.Create(1280, 720, "Proiect");
}

void setWindowCallbacks() {
    glfwSetWindowSizeCallback(myWindow.getWindow(), windowResizeCallback);
    glfwSetKeyCallback(myWindow.getWindow(), keyboardCallback);
    glfwSetCursorPosCallback(myWindow.getWindow(), mouseCallback);
    glfwSetInputMode(myWindow.getWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

void initOpenGLState() {
    glClearColor(0.7f, 0.7f, 0.7f, 1.0f);
    glViewport(0, 0, myWindow.getWindowDimensions().width, myWindow.getWindowDimensions().height);
    glEnable(GL_FRAMEBUFFER_SRGB);
    glEnable(GL_DEPTH_TEST); // enable depth-testing
    glDepthFunc(GL_LESS); // depth-testing interprets a smaller value as "closer"
    glEnable(GL_CULL_FACE); // cull face
    glCullFace(GL_BACK); // cull back face
    glFrontFace(GL_CCW); // GL_CCW for counter clock-wise
    glPointSize(2.0f); // vertex size in point (polygonal) mode
}

void initModels() {
    terrain.LoadModel("models/brutalism/terrain.obj");
    satuMare.LoadModel("models/brutalism/satu_mare.obj");
    genex.LoadModel("models/brutalism/genex.obj");
    genexRestaurant.LoadModel("models/brutalism/genex_restaurant.obj");
    panorama.LoadModel("models/brutalism/panorama.obj");
    telefoane.LoadModel("models/brutalism/telefoane.obj");
    lampPost.LoadModel("models/brutalism/lamp_post.obj");
    lampBulb.LoadModel("models/brutalism/lamp_bulb.obj");
    floodlight.LoadModel("models/brutalism/floodlight.obj");

    // collision shapes, computed once from the meshes
    satuMareBounds = satuMare.GetMeshBounds();
    genexBounds = genex.GetMeshBounds();
    restaurantBounds = genexRestaurant.GetMeshBounds();
    panoramaBounds = panorama.GetMeshBounds();
    telefoaneBounds = telefoane.GetMeshBounds();
    lampPostBounds = lampPost.GetMeshBounds();
    floodlightBounds = floodlight.GetMeshBounds();
}

void initShaders() {
    myBasicShader.loadShader(
        "shaders/basic.vert",
        "shaders/basic.frag");
    rainShader.loadShader(
        "shaders/rain.vert",
        "shaders/rain.frag");
}

void initRain() {
    // each drop: 2 vertices with the same random seed, w = 0 (bottom) / 1 (top)
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> random01(0.0f, 1.0f);
    std::vector<glm::vec4> vertices;
    for (int i = 0; i < RAIN_DROPS; i++) {
        glm::vec3 seed(random01(rng), random01(rng), random01(rng));
        vertices.push_back(glm::vec4(seed, 0.0f));
        vertices.push_back(glm::vec4(seed, 1.0f));
    }

    glGenVertexArrays(1, &rainVAO);
    glBindVertexArray(rainVAO);
    glGenBuffers(1, &rainVBO);
    glBindBuffer(GL_ARRAY_BUFFER, rainVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec4), vertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), (GLvoid*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    GLuint program = rainShader.shaderProgram;
    rainViewLoc = glGetUniformLocation(program, "view");
    rainProjectionLoc = glGetUniformLocation(program, "projection");
    rainCameraPosLoc = glGetUniformLocation(program, "cameraPos");
    rainTimeLoc = glGetUniformLocation(program, "time");
    rainFallSpeedLoc = glGetUniformLocation(program, "fallSpeed");
    rainWindLoc = glGetUniformLocation(program, "wind");
    rainAreaSizeLoc = glGetUniformLocation(program, "areaSize");
    rainAreaHeightLoc = glGetUniformLocation(program, "areaHeight");
    rainDropLengthLoc = glGetUniformLocation(program, "dropLength");
    rainColorLoc = glGetUniformLocation(program, "rainColor");
}

void renderRain() {
    if (!rainEnabled) return;

    rainShader.useShaderProgram();
    glm::vec3 cameraPos = glm::vec3(glm::inverse(view)[3]); // camera position = translation of the inverse view
    glm::vec2 wind = windSpeed * glm::vec2(cos(glm::radians(windDirection)), sin(glm::radians(windDirection)));
    // drops reflect the sky: lighter by day, dim at night
    glm::vec4 color = nightMode ? glm::vec4(0.45f, 0.5f, 0.6f, 0.35f) : glm::vec4(0.8f, 0.85f, 0.9f, 0.45f);

    glUniformMatrix4fv(rainViewLoc, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(rainProjectionLoc, 1, GL_FALSE, glm::value_ptr(projection));
    glUniform3fv(rainCameraPosLoc, 1, glm::value_ptr(cameraPos));
    glUniform1f(rainTimeLoc, (float)glfwGetTime());
    glUniform1f(rainFallSpeedLoc, RAIN_FALL_SPEED);
    glUniform2fv(rainWindLoc, 1, glm::value_ptr(wind));
    glUniform1f(rainAreaSizeLoc, RAIN_AREA_SIZE);
    glUniform1f(rainAreaHeightLoc, RAIN_AREA_HEIGHT);
    glUniform1f(rainDropLengthLoc, RAIN_DROP_LENGTH);
    glUniform4fv(rainColorLoc, 1, glm::value_ptr(color));

    // transparent: blend over the scene, test against its depth (no rain inside buildings),
    // but don't write depth, so drops never hide each other
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glBindVertexArray(rainVAO);
    glDrawArrays(GL_LINES, 0, RAIN_DROPS * 2);
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void initUniforms() {
    myBasicShader.useShaderProgram();

    modelLoc = glGetUniformLocation(myBasicShader.shaderProgram, "model");

    // get view matrix for current camera
    view = myCamera.getViewMatrix();
    viewLoc = glGetUniformLocation(myBasicShader.shaderProgram, "view");
    // send view matrix to shader
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));

    normalMatrixLoc = glGetUniformLocation(myBasicShader.shaderProgram, "normalMatrix");

    // create projection matrix
    projection = glm::perspective(glm::radians(45.0f),
        (float)myWindow.getWindowDimensions().width / (float)myWindow.getWindowDimensions().height,
        0.5f, 1500.0f);
    projectionLoc = glGetUniformLocation(myBasicShader.shaderProgram, "projection");
    // send projection matrix to shader
    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));

    //set the light direction (direction towards the light)
    lightDir = glm::vec3(0.0f, 1.0f, 1.0f);
    lightDirLoc = glGetUniformLocation(myBasicShader.shaderProgram, "lightDir");
    // send light dir to shader
    glUniform3fv(lightDirLoc, 1, glm::value_ptr(lightDir));

    //set light color
    lightColor = glm::vec3(1.0f, 1.0f, 1.0f); //white light
    lightColorLoc = glGetUniformLocation(myBasicShader.shaderProgram, "lightColor");
    // send light color to shader
    glUniform3fv(lightColorLoc, 1, glm::value_ptr(lightColor));

    // street lamps: positions are fixed, so they are sent once (world space)
    std::vector<glm::vec3> lightPositions;
    for (const glm::vec3& base : LAMP_POSITIONS) {
        lightPositions.push_back(base + glm::vec3(0.0f, LAMP_LIGHT_HEIGHT, 0.0f));
    }
    pointLightPosLoc = glGetUniformLocation(myBasicShader.shaderProgram, "pointLightPos");
    glUniform3fv(pointLightPosLoc, (GLsizei)lightPositions.size(), glm::value_ptr(lightPositions[0]));
    pointLightColorLoc = glGetUniformLocation(myBasicShader.shaderProgram, "pointLightColor");
    glUniform3fv(pointLightColorLoc, 1, glm::value_ptr(LAMP_COLOR));
    numPointLightsLoc = glGetUniformLocation(myBasicShader.shaderProgram, "numPointLights");
    emissiveColorLoc = glGetUniformLocation(myBasicShader.shaderProgram, "emissiveColor");

    // floodlights: fixed, sent once (world space)
    std::vector<glm::vec3> spotPositions, spotDirections;
    for (const SpotLight& spot : SPOT_LIGHTS) {
        spotPositions.push_back(spot.position);
        spotDirections.push_back(glm::normalize(spot.target - spot.position));
    }
    spotLightPosLoc = glGetUniformLocation(myBasicShader.shaderProgram, "spotLightPos");
    glUniform3fv(spotLightPosLoc, (GLsizei)spotPositions.size(), glm::value_ptr(spotPositions[0]));
    spotLightDirLoc = glGetUniformLocation(myBasicShader.shaderProgram, "spotLightDir");
    glUniform3fv(spotLightDirLoc, (GLsizei)spotDirections.size(), glm::value_ptr(spotDirections[0]));
    spotLightColorLoc = glGetUniformLocation(myBasicShader.shaderProgram, "spotLightColor");
    glUniform3fv(spotLightColorLoc, 1, glm::value_ptr(SPOT_COLOR));
    // the shader compares cosines, so the angles are sent as cos(angle)
    spotCutOffLoc = glGetUniformLocation(myBasicShader.shaderProgram, "spotCutOff");
    glUniform1f(spotCutOffLoc, cos(glm::radians(SPOT_INNER_ANGLE)));
    spotOuterCutOffLoc = glGetUniformLocation(myBasicShader.shaderProgram, "spotOuterCutOff");
    glUniform1f(spotOuterCutOffLoc, cos(glm::radians(SPOT_OUTER_ANGLE)));
    numSpotLightsLoc = glGetUniformLocation(myBasicShader.shaderProgram, "numSpotLights");

    flatShadingLoc = glGetUniformLocation(myBasicShader.shaderProgram, "flatShading");
    glUniform1i(flatShadingLoc, flatShading);

    fogDensityLoc = glGetUniformLocation(myBasicShader.shaderProgram, "fogDensity");
    fogColorLoc = glGetUniformLocation(myBasicShader.shaderProgram, "fogColor");
    applyFog();

    applyLighting();
}

// draws a model with its own model matrix; normalMatrix is computed per object
void renderObject(gps::Model3D& object, const glm::mat4& objectModel, gps::Shader shader,
    const glm::vec3& emissive = glm::vec3(0.0f)) {
    shader.useShaderProgram();
    glUniform3fv(emissiveColorLoc, 1, glm::value_ptr(emissive));
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(objectModel));
    glm::mat3 nm = glm::mat3(glm::inverseTranspose(view * objectModel));
    glUniformMatrix3fv(normalMatrixLoc, 1, GL_FALSE, glm::value_ptr(nm));
    object.Draw(shader);
}

void renderScene() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // brutalist scene (positions in meters)
    glm::mat4 identity(1.0f);
    renderObject(terrain, identity, myBasicShader);
    renderObject(satuMare, satuMareMatrix(), myBasicShader);
    renderObject(genex, genexMatrix(), myBasicShader);
    renderObject(genexRestaurant, restaurantMatrix(), myBasicShader);
    renderObject(panorama, panoramaMatrix(), myBasicShader);
    renderObject(telefoane, telefoaneMatrix(), myBasicShader);

    // street lamps; the bulbs glow at night
    glm::vec3 bulbGlow = nightMode ? LAMP_GLOW : glm::vec3(0.0f);
    for (const glm::vec3& base : LAMP_POSITIONS) {
        glm::mat4 lampModel = glm::translate(identity, base);
        renderObject(lampPost, lampModel, myBasicShader);
        renderObject(lampBulb, lampModel, myBasicShader, bulbGlow);
    }
    for (const SpotLight& spot : SPOT_LIGHTS) {
        renderObject(floodlight, glm::translate(identity, spot.position), myBasicShader, bulbGlow);
    }
}

void initUI() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr; // don't write imgui.ini next to the exe
    ImGui::StyleColorsDark();
    // must run after setWindowCallbacks(): ImGui chains to our callbacks instead of replacing them
    ImGui_ImplGlfw_InitForOpenGL(myWindow.getWindow(), true);
    ImGui_ImplOpenGL3_Init("#version 410");
}

void keyRow(const char* keys, const char* action) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn(); ImGui::TextUnformatted(keys);
    ImGui::TableNextColumn(); ImGui::TextUnformatted(action);
}

void renderUI() {
    // with the cursor captured the (invisible) mouse must not click the UI
    ImGuiIO& io = ImGui::GetIO();
    bool cursorFree = glfwGetInputMode(myWindow.getWindow(), GLFW_CURSOR) == GLFW_CURSOR_NORMAL;
    if (cursorFree) io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
    else io.ConfigFlags |= ImGuiConfigFlags_NoMouse;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    if (showUI) {
        ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.7f);
        ImGui::Begin("Controls", nullptr,
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);

        ImGui::Text("FPS: %.0f (%.1f ms/frame)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
        ImGui::TextDisabled("GPU: %s", (const char*)glGetString(GL_RENDERER));

        ImGui::SeparatorText("View mode");
        bool changed = false;
        changed |= ImGui::RadioButton("Solid", &viewMode, VIEW_SOLID);
        ImGui::SameLine();
        changed |= ImGui::RadioButton("Wireframe", &viewMode, VIEW_WIREFRAME);
        ImGui::SameLine();
        changed |= ImGui::RadioButton("Points", &viewMode, VIEW_POINTS);
        if (changed) applyViewMode();
        if (ImGui::Checkbox("Flat shading", &flatShading)) applyShadingMode();

        ImGui::SeparatorText("Lighting");
        if (ImGui::Checkbox("Night (street lamps + floodlights)", &nightMode)) applyLighting();

        ImGui::SeparatorText("Fog");
        if (ImGui::Checkbox("Fog", &fogEnabled)) applyFog();
        if (ImGui::SliderFloat("Density", &fogDensity, 0.0005f, 0.01f, "%.4f")) applyFog();

        ImGui::SeparatorText("Weather");
        ImGui::Checkbox("Rain", &rainEnabled);
        ImGui::SliderFloat("Wind speed", &windSpeed, 0.0f, 20.0f, "%.0f m/s");
        ImGui::SliderFloat("Wind direction", &windDirection, 0.0f, 360.0f, "%.0f deg");

        ImGui::SeparatorText("Camera");
        ImGui::Checkbox("Collisions", &collisionsEnabled);

        ImGui::SeparatorText("Camera tour");
        if (tourActive) {
            ImGui::ProgressBar(tourTime / tourKeyTimes.back(), ImVec2(200.0f, 0.0f));
            if (ImGui::Button("Stop tour")) stopTour();
        }
        else if (ImGui::Button("Start tour")) {
            startTour();
        }

        ImGui::SeparatorText("Palatul Telefoanelor");
        ImGui::Text("Position: %.0f, %.0f", telefoanePos.x, telefoanePos.z);
        ImGui::Text("Rotation: %.0f deg   Scale: %.2f", fmod(telefoaneAngle + 360.0f, 360.0f), telefoaneScale);
        if (ImGui::Button("Reset building")) resetTelefoane();

        ImGui::SeparatorText("Key map");
        if (ImGui::BeginTable("keymap", 2, ImGuiTableFlags_SizingFixedFit)) {
            keyRow("W A S D", "Move camera");
            keyRow("Shift", "Move faster");
            keyRow("Mouse", "Look around");
            keyRow("M", "Free / capture cursor");
            keyRow("1 / 2 / 3", "Solid / wireframe / points");
            keyRow("4", "Smooth / flat shading");
            keyRow("Arrows", "Move building");
            keyRow("Q / E", "Rotate building");
            keyRow("Z / X", "Scale building down / up");
            keyRow("R", "Reset building");
            keyRow("N", "Day / night");
            keyRow("F", "Fog on / off");
            keyRow("P", "Rain on / off");
            keyRow("C", "Collisions on / off");
            keyRow("T", "Start / stop camera tour");
            keyRow("H", "Hide / show this panel");
            keyRow("Esc", "Exit");
            ImGui::EndTable();
        }
        ImGui::TextDisabled(cursorFree ? "Press M to control the camera" : "Press M to use the buttons");
        ImGui::End();
    }

    ImGui::Render();
    // ImGui colors are already sRGB: drawing them through the sRGB framebuffer would wash them out
    glDisable(GL_FRAMEBUFFER_SRGB);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glEnable(GL_FRAMEBUFFER_SRGB);
}

void cleanup() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    myWindow.Delete();
    //cleanup code for your own data
}

int main(int argc, const char* argv[]) {

    try {
        initOpenGLWindow();
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    initOpenGLState();
    initModels();
    initShaders();
    initUniforms();
    initRain();
    setWindowCallbacks();
    initUI();
    initTour();

    glCheckError();
    // application loop
    lastFrameTime = glfwGetTime();
    while (!glfwWindowShouldClose(myWindow.getWindow())) {
        double now = glfwGetTime();
        deltaTime = (float)(now - lastFrameTime);
        lastFrameTime = now;

        processMovement();
        updateTour();
        processObjectTransform();
        updateAnimations();
        renderScene();
        renderRain();
        renderUI();

        glfwPollEvents();
        glfwSwapBuffers(myWindow.getWindow());

        glCheckError();
    }

    cleanup();

    return EXIT_SUCCESS;
}