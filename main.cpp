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
#include <glm/gtc/constants.hpp> //glm::two_pi

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
#include <algorithm>
#include <fstream>
#include <map>
#include <string>

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
    {-455.0f, 0.0f, 186.0f}, {-385.0f, 0.0f, 186.0f},
};
// floodlights (spot lights) on the Administrative Palace, on at night
struct SpotLight {
    glm::vec3 position;
    glm::vec3 target;   // point the floodlight is aimed at
};
const std::vector<SpotLight> SPOT_LIGHTS = { // max 4, see basic.frag
    {{-5.0f, 18.3f, 88.0f}, {0.0f, 70.0f, 68.0f}},  // on the roof of the low wing (y = 16), aimed at the main tower
    {{5.0f, 18.3f, 88.0f},  {0.0f, 70.0f, 68.0f}},  // (position = lamp heads; the tripod stands 2.3 m below)
    {{25.0f, 2.3f, 90.0f},  {19.0f, 40.0f, 72.0f}}, // on the plaza, aimed at the second block
};
const glm::vec3 SPOT_COLOR(5.0f, 5.0f, 5.5f);  // cool white, strong enough to reach the top of the tower
const float SPOT_INNER_ANGLE = 10.0f;          // degrees: full intensity inside
const float SPOT_OUTER_ANGLE = 15.0f;          // degrees: fades out between inner and outer
gps::Model3D floodlight;
gps::Model3D floodlightLens;   // only the lenses glow at night
gps::Model3D lampPost;
gps::Model3D lampBulb;

// camera tour: the camera follows a smooth path through these key points
struct TourKey {
    glm::vec3 position;
    glm::vec3 target;   // point the camera looks at
};
const std::vector<TourKey> TOUR = {
    {{40.0f, 45.0f, 420.0f},    {0.0f, 30.0f, 60.0f}},      // overview from the south
    {{0.0f, 12.0f, 240.0f},     {0.0f, 20.0f, 60.0f}},      // approach the plaza
    {{-30.0f, 6.0f, 150.0f},    {0.0f, 60.0f, 60.0f}},      // look up at the Administrative Palace
    {{-120.0f, 15.0f, 215.0f},  {-170.0f, 10.0f, 140.0f}},  // Palatul Telefoanelor
    {{-300.0f, 10.0f, 196.0f},  {-420.0f, 40.0f, 196.0f}},  // west along the road, towards Genex
    {{-520.0f, 8.0f, 196.0f},   {-640.0f, 10.0f, 230.0f}},  // through the gate, into the exclusion zone
    {{-590.0f, 30.0f, 390.0f},  {-690.0f, 15.0f, 300.0f}},  // Pripyat and the Ferris wheel
    {{-650.0f, 60.0f, 190.0f},  {-780.0f, 30.0f, 70.0f}},   // the power plant
    {{-680.0f, 75.0f, -150.0f}, {-700.0f, 60.0f, -330.0f}}, // over the river, the Duga radar
    {{-300.0f, 110.0f, -330.0f}, {-100.0f, 80.0f, -520.0f}},// along the north bank, Hotel Panorama in the mountains
    {{260.0f, 75.0f, -70.0f},   {450.0f, 60.0f, -150.0f}},  // Motherland Calls on her hill
    {{560.0f, 70.0f, 40.0f},    {0.0f, 30.0f, 100.0f}},     // look back over the river at the city
    {{40.0f, 45.0f, 420.0f},    {0.0f, 30.0f, 60.0f}},      // back to the start
};
const float TOUR_SPEED = 35.0f;       // meters per second along the path
const float TOUR_MIN_SEGMENT = 3.0f;  // seconds, so short segments are not rushed
std::vector<float> tourKeyTimes;      // time at which the camera reaches each key point
bool tourActive = false;
float tourTime = 0.0f;

// fog: distant objects fade into the sky color
const glm::vec3 DAY_SKY(0.7f, 0.7f, 0.7f);
const glm::vec3 NIGHT_SKY(0.02f, 0.03f, 0.06f);
bool fogEnabled = true;
float fogDensity = 0.0012f;   // the world is 2 km wide: lighter fog than before
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
const float CAMERA_MIN_HEIGHT = 1.8f;  // eye height above the ground (terrain height map)
bool collisionsEnabled = true;
std::vector<gps::BoundingBox> satuMareBounds, genexBounds, restaurantBounds, panoramaBounds,
                              telefoaneBounds, lampPostBounds, floodlightBounds; // model space, from the meshes
std::vector<gps::BoundingBox> worldColliders; // world space, rebuilt every frame (some objects move)

// traffic: cars are created at one end of the road while the program runs and removed at the other
struct Car {
    float x;      // position along the road (the road runs along X)
    float speed;  // meters per second
    int lane;     // 0 = eastbound (+X), 1 = westbound (-X)
    int model;    // index in carModels (body color)
};
const int CAR_COLORS = 4;
const float ROAD_END = 1000.0f;                  // the road ends with the terrain; the fog hides it
const float LANE_Z[2] = { 199.0f, 193.0f };      // right-hand traffic
const float LANE_DIR[2] = { 1.0f, -1.0f };
const float ROAD_Y = 0.25f;
const float CAR_MIN_GAP = 12.0f;                 // meters to the car in front
bool trafficEnabled = true;
std::vector<Car> cars;
float nextSpawnTime[2] = { 0.0f, 0.0f };
std::mt19937 trafficRng(7);
gps::Model3D carModels[CAR_COLORS];
gps::Model3D carLights;   // head- and taillights, separate so they can glow at night

// more landmarks: Motherland Calls (Volgograd), Romanita (Chisinau), Chornobyl (power plant, Duga radar,
// Pripyat apartment blocks and the Ferris wheel from the Pripyat amusement park)
gps::Model3D motherland, romanita, powerPlant, duga, pripyatBlocks;
// level of detail (course 09 - Models, mesh simplification): simplified copies of the heaviest models,
// drawn instead of the full ones when the camera is far away (the colliders still use the full models)
gps::Model3D motherlandLod, powerPlantLod, dugaLod, pripyatBlocksLod;
const float LOD_DISTANCE = 250.0f;   // meters from the camera to the nearest point of the model
gps::Model3D ferrisSupport, ferrisWheel, ferrisGondola;
// layout: the Romanian/Moldovan city center on the south bank (palace, Palatul Telefoanelor, Romanita),
// Genex as the western gate of the city on the main road, Motherland Calls on a hill above the river,
// Hotel Panorama in the mountains, and the Chornobyl exclusion zone in the forest beyond the gate
// (positions must match tools/gen_world.py, which flattens the ground under each building)
const glm::vec3 GENEX_POS(-420.0f, 0.0f, 196.0f);
const glm::vec3 RESTAURANT_OFFSET(0.0f, 108.0f, 33.0f);  // from the base of Genex
const glm::vec3 PANORAMA_POS(-100.0f, 70.0f, -520.0f);
const glm::vec3 MOTHERLAND_POS(450.0f, 35.0f, -150.0f);
const glm::vec3 ROMANITA_POS(150.0f, 0.0f, 300.0f);
const glm::vec3 POWER_PLANT_POS(-780.0f, 0.0f, 70.0f);
const glm::vec3 DUGA_POS(-700.0f, 0.0f, -330.0f);
const glm::vec3 PRIPYAT_POS(-780.0f, 0.0f, 250.0f);
const glm::vec3 FERRIS_POS(-640.0f, 0.0f, 320.0f);
gps::Model3D trees;   // distant forest: crossed billboard quads, all in one model
// trees near the tour path: one full 3D model per species, drawn at every position from trees_near.txt
const char* TREE_SPECIES[] = { "fir", "pine", "noble", "dead", "dead2" };
const int TREE_SPECIES_COUNT = 5;
gps::Model3D treeModels[TREE_SPECIES_COUNT];
struct NearTree {
    int species;
    glm::mat4 model;
};
std::vector<NearTree> nearTrees;
const float FERRIS_HUB_HEIGHT = 14.63f;   // must match ferris_support.obj / ferris_wheel.obj
const float FERRIS_RADIUS = 11.2f;
const int FERRIS_GONDOLAS = 20;
const float FERRIS_SPEED = 4.0f;         // degrees per second
float ferrisAngle = 0.0f;
std::vector<gps::BoundingBox> motherlandBounds, romanitaBounds, powerPlantBounds, dugaBounds, pripyatBounds,
                              ferrisSupportBounds, ferrisWheelBounds;

// terrain height map (written by tools/gen_world.py together with the terrain),
// used to keep the camera above the ground: heights on a regular grid, read once at startup
struct HeightMap {
    float x0 = 0.0f, z0 = 0.0f, step = 1.0f;   // world position of the first sample, grid spacing
    int nx = 0, nz = 0;
    std::vector<float> heights;                // nz rows of nx values
};
HeightMap terrainHeights;

// antialiasing: the scene is drawn into a multisampled framebuffer of our own, resolved into a texture
// and copied to the screen. The window's own framebuffer stays single-sampled (MSAA on it got corrupted
// on maximize with the NVIDIA driver); our framebuffer is recreated, under our control, on every resize.
const int MSAA_SAMPLES = 4;
bool antialiasingEnabled = true;
GLuint msaaFBO = 0, msaaColorRBO = 0, msaaDepthRBO = 0;   // multisampled color + depth
GLuint resolveFBO = 0, resolveTexture = 0;                // single-sampled result, read by the screen pass
int sceneWidth = 0, sceneHeight = 0;
gps::Shader screenShader;
GLuint screenVAO = 0;                                      // empty: the fullscreen triangle comes from gl_VertexID

// shadow mapping (directional light): the scene is first drawn from the sun into a depth texture;
// a fragment is in shadow if something closer to the sun covers it in that texture
const int SHADOW_MAP_SIZE = 4096;
const float SHADOW_AREA = 800.0f;        // meters covered by the shadow map, centered on the camera
const float SHADOW_DEPTH_RANGE = 800.0f; // meters in front of / behind the center, along the sun's direction
bool shadowsEnabled = true;
GLuint depthMapFBO = 0, depthMapTexture = 0;
gps::Shader depthMapShader;
GLint depthModelLoc, depthLightSpaceLoc;           // in depthMapShader
GLint lightSpaceTrMatrixLoc, shadowMapLoc, shadowsEnabledLoc; // in myBasicShader
glm::mat4 lightSpaceTrMatrix;
bool depthPass = false;                  // true while drawing into the shadow map
// performance: the shadow map is redrawn every SHADOW_UPDATE_INTERVAL frames (the sun doesn't move, so it barely
// changes from one frame to the next) and small objects far from the camera are left out of it
const int SHADOW_UPDATE_INTERVAL = 2;
const float SHADOW_DETAIL_DISTANCE = 200.0f;  // meters: trees and cars farther than this cast no shadow
int framesSinceShadowUpdate = SHADOW_UPDATE_INTERVAL;
const int SHADOW_MAP_TEXTURE_UNIT = 5;   // units 0-2 are used by the models' own textures

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

void deleteSceneFramebuffers() {
    if (msaaFBO) glDeleteFramebuffers(1, &msaaFBO);
    if (msaaColorRBO) glDeleteRenderbuffers(1, &msaaColorRBO);
    if (msaaDepthRBO) glDeleteRenderbuffers(1, &msaaDepthRBO);
    if (resolveFBO) glDeleteFramebuffers(1, &resolveFBO);
    if (resolveTexture) glDeleteTextures(1, &resolveTexture);
    msaaFBO = msaaColorRBO = msaaDepthRBO = resolveFBO = resolveTexture = 0;
}

// (re)creates the framebuffers at the window size; samples = 0 means no antialiasing
void createSceneFramebuffers(int width, int height) {
    deleteSceneFramebuffers();
    sceneWidth = width;
    sceneHeight = height;

    GLint maxSamples = 0;
    glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
    int samples = antialiasingEnabled ? glm::min(MSAA_SAMPLES, (int)maxSamples) : 0;

    // RGBA16F: the scene stays in linear colors (no banding in the dark night scenes);
    // the conversion to sRGB happens once, when the result is copied to the screen
    glGenRenderbuffers(1, &msaaColorRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, msaaColorRBO);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_RGBA16F, width, height);
    glGenRenderbuffers(1, &msaaDepthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, msaaDepthRBO);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH_COMPONENT24, width, height);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    glGenFramebuffers(1, &msaaFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, msaaFBO);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, msaaColorRBO);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, msaaDepthRBO);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "The multisampled framebuffer is incomplete" << std::endl;
    }

    glGenTextures(1, &resolveTexture);
    glBindTexture(GL_TEXTURE_2D, resolveTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenFramebuffers(1, &resolveFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, resolveFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, resolveTexture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "The resolve framebuffer is incomplete" << std::endl;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void windowResizeCallback(GLFWwindow* window, int width, int height) {
    fprintf(stdout, "Window resized! New width: %d , and height: %d\n", width, height);
    if (width == 0 || height == 0) return; // window minimized
    firstMouse = true; // the cursor is re-centered on resize; don't treat that as mouse movement

    int fbWidth, fbHeight;
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    myWindow.setWindowDimensions(WindowDimensions{ fbWidth, fbHeight });
    glViewport(0, 0, fbWidth, fbHeight);
    createSceneFramebuffers(fbWidth, fbHeight);

    projection = glm::perspective(glm::radians(45.0f), (float)fbWidth / (float)fbHeight, 0.5f, 3000.0f);
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
glm::mat4 genexMatrix() { return glm::translate(glm::mat4(1.0f), GENEX_POS); }
glm::mat4 panoramaMatrix() { return glm::translate(glm::mat4(1.0f), PANORAMA_POS); }

glm::mat4 restaurantMatrix() {
    // rotate around its own axis first, then move it on top of the tower (T * R)
    glm::mat4 m = glm::translate(glm::mat4(1.0f), GENEX_POS + RESTAURANT_OFFSET);
    return glm::rotate(m, glm::radians(restaurantAngle), glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 ferrisWheelMatrix() {
    // the wheel turns around its horizontal axle (Z), at the height of the hub
    glm::mat4 m = glm::translate(glm::mat4(1.0f), FERRIS_POS + glm::vec3(0.0f, FERRIS_HUB_HEIGHT, 0.0f));
    return glm::rotate(m, glm::radians(ferrisAngle), glm::vec3(0.0f, 0.0f, 1.0f));
}

// the gondolas hang from the rim: they follow the wheel, but only by translation, so they stay upright
glm::mat4 ferrisGondolaMatrix(int i) {
    float angle = glm::radians(ferrisAngle) + glm::two_pi<float>() * i / FERRIS_GONDOLAS;
    glm::vec3 hub = FERRIS_POS + glm::vec3(0.0f, FERRIS_HUB_HEIGHT, 0.0f);
    return glm::translate(glm::mat4(1.0f), hub + FERRIS_RADIUS * glm::vec3(cos(angle), sin(angle), 0.0f));
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
    addColliders(motherlandBounds, glm::translate(glm::mat4(1.0f), MOTHERLAND_POS));
    addColliders(romanitaBounds, glm::translate(glm::mat4(1.0f), ROMANITA_POS));
    addColliders(powerPlantBounds, glm::translate(glm::mat4(1.0f), POWER_PLANT_POS));
    addColliders(dugaBounds, glm::translate(glm::mat4(1.0f), DUGA_POS));
    addColliders(pripyatBounds, glm::translate(glm::mat4(1.0f), PRIPYAT_POS));
    addColliders(ferrisSupportBounds, glm::translate(glm::mat4(1.0f), FERRIS_POS));
    addColliders(ferrisWheelBounds, ferrisWheelMatrix());
    for (const glm::vec3& base : LAMP_POSITIONS) {
        addColliders(lampPostBounds, glm::translate(glm::mat4(1.0f), base));
    }
    for (const SpotLight& spot : SPOT_LIGHTS) {
        addColliders(floodlightBounds, glm::translate(glm::mat4(1.0f), spot.position));
    }
}

// one line per tree: species x y z yaw(degrees) scale
bool loadNearTrees(const char* path) {
    std::ifstream in(path);
    if (!in) {
        std::cerr << "Could not open " << path << std::endl;
        return false;
    }
    std::string name;
    while (in >> name) {
        if (name[0] == '#') { std::getline(in, name); continue; }   // comment line
        float x, y, z, yaw, scale;
        in >> x >> y >> z >> yaw >> scale;
        for (int i = 0; i < TREE_SPECIES_COUNT; i++) {
            if (name == TREE_SPECIES[i]) {
                glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, z));
                m = glm::rotate(m, glm::radians(yaw), glm::vec3(0.0f, 1.0f, 0.0f));
                nearTrees.push_back({ i, glm::scale(m, glm::vec3(scale)) });
            }
        }
    }
    return true;
}

bool loadHeightMap(const char* path) {
    std::ifstream in(path);
    if (!in) {
        std::cerr << "Could not open the height map " << path << std::endl;
        return false;
    }
    HeightMap& m = terrainHeights;
    in >> m.x0 >> m.z0 >> m.step >> m.nx >> m.nz;
    m.heights.resize((size_t)m.nx * m.nz);
    for (float& h : m.heights) in >> h;
    return (bool)in;
}

// ground height under (x, z): bilinear interpolation between the 4 nearest samples
float groundHeight(float x, float z) {
    const HeightMap& m = terrainHeights;
    if (m.heights.empty()) return 0.0f;
    float fx = glm::clamp((x - m.x0) / m.step, 0.0f, (float)(m.nx - 1) - 0.001f);
    float fz = glm::clamp((z - m.z0) / m.step, 0.0f, (float)(m.nz - 1) - 0.001f);
    int i = (int)fx, j = (int)fz;
    float tx = fx - i, tz = fz - j;
    float h00 = m.heights[j * m.nx + i], h10 = m.heights[j * m.nx + i + 1];
    float h01 = m.heights[(j + 1) * m.nx + i], h11 = m.heights[(j + 1) * m.nx + i + 1];
    return glm::mix(glm::mix(h00, h10, tx), glm::mix(h01, h11, tx), tz);
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
    newPos.y = glm::max(newPos.y, groundHeight(newPos.x, newPos.z) + CAMERA_MIN_HEIGHT);
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

Car randomCar(int lane, float x) {
    std::uniform_real_distribution<float> speed(11.0f, 19.0f);   // ~40-70 km/h
    std::uniform_int_distribution<int> color(0, CAR_COLORS - 1);
    return Car{ x, speed(trafficRng), lane, color(trafficRng) };
}

float randomSpawnDelay() {
    std::uniform_real_distribution<float> delay(2.0f, 6.0f);     // seconds between two cars in a lane
    return delay(trafficRng);
}

// at startup the road is filled along its whole length, so it isn't empty while the first cars arrive
void initTraffic() {
    cars.clear();
    for (int lane = 0; lane < 2; lane++) {
        float distance = 0.0f;
        while (distance < 2.0f * ROAD_END) {
            Car car = randomCar(lane, 0.0f);
            car.x = LANE_DIR[lane] * (distance - ROAD_END);   // from the lane's start towards its end
            cars.push_back(car);
            distance += car.speed * randomSpawnDelay();
        }
        nextSpawnTime[lane] = (float)glfwGetTime() + randomSpawnDelay();
    }
}

void updateTraffic() {
    if (!trafficEnabled) return;
    float now = (float)glfwGetTime();

    // spawn: a new car at the start of each lane, unless the previous one is still too close
    for (int lane = 0; lane < 2; lane++) {
        if (now < nextSpawnTime[lane]) continue;
        float start = -LANE_DIR[lane] * ROAD_END;
        bool startIsFree = true;
        for (const Car& car : cars) {
            if (car.lane == lane && fabs(car.x - start) < CAR_MIN_GAP) startIsFree = false;
        }
        if (startIsFree) {
            cars.push_back(randomCar(lane, start));
            nextSpawnTime[lane] = now + randomSpawnDelay();
        }
    }

    // move: a car slows down to the speed of the car in front if it gets too close
    for (Car& car : cars) {
        float speed = car.speed;
        for (const Car& other : cars) {
            float ahead = (other.x - car.x) * LANE_DIR[car.lane];
            if (other.lane == car.lane && ahead > 0.0f && ahead < CAR_MIN_GAP) {
                speed = glm::min(speed, other.speed);
            }
        }
        car.x += LANE_DIR[car.lane] * speed * deltaTime;
    }

    // remove the cars that reached the end of the road
    cars.erase(std::remove_if(cars.begin(), cars.end(),
        [](const Car& car) { return car.x * LANE_DIR[car.lane] > ROAD_END; }), cars.end());
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
        if (key == GLFW_KEY_V) trafficEnabled = !trafficEnabled;
        if (key == GLFW_KEY_O) shadowsEnabled = !shadowsEnabled;
        if (key == GLFW_KEY_B) {
            antialiasingEnabled = !antialiasingEnabled;
            createSceneFramebuffers(sceneWidth, sceneHeight);
        }
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
    ferrisAngle = fmod(ferrisAngle + FERRIS_SPEED * deltaTime, 360.0f);
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
    terrain.LoadModel("models/terrain/terrain.obj");
    satuMare.LoadModel("models/satu_mare/satu_mare.obj");
    genex.LoadModel("models/genex/genex.obj");
    genexRestaurant.LoadModel("models/genex/genex_restaurant.obj");
    panorama.LoadModel("models/panorama/panorama.obj");
    telefoane.LoadModel("models/telefoane/telefoane.obj");
    lampPost.LoadModel("models/lamp_post/lamp_post.obj");
    lampBulb.LoadModel("models/lamp_post/lamp_bulb.obj");
    floodlight.LoadModel("models/floodlight/floodlight.obj");
    floodlightLens.LoadModel("models/floodlight/floodlight_lens.obj");
    const char* carFiles[CAR_COLORS] = {
        "models/cars/car_red.obj", "models/cars/car_blue.obj",
        "models/cars/car_white.obj", "models/cars/car_yellow.obj" };
    for (int i = 0; i < CAR_COLORS; i++) {
        carModels[i].LoadModel(carFiles[i]);
    }
    carLights.LoadModel("models/cars/car_lights.obj");
    motherland.LoadModel("models/motherland/motherland.obj");
    romanita.LoadModel("models/romanita/romanita.obj");
    powerPlant.LoadModel("models/power_plant/power_plant.obj");
    duga.LoadModel("models/duga/duga.obj");
    pripyatBlocks.LoadModel("models/pripyat/pripyat_blocks.obj");
    motherlandLod.LoadModel("models/motherland/motherland_lod.obj");
    powerPlantLod.LoadModel("models/power_plant/power_plant_lod.obj");
    dugaLod.LoadModel("models/duga/duga_lod.obj");
    pripyatBlocksLod.LoadModel("models/pripyat/pripyat_blocks_lod.obj");
    ferrisSupport.LoadModel("models/ferris_wheel/ferris_support.obj");
    ferrisWheel.LoadModel("models/ferris_wheel/ferris_wheel.obj");
    ferrisGondola.LoadModel("models/ferris_wheel/ferris_gondola.obj");
    trees.LoadModel("models/trees/trees.obj");
    for (int i = 0; i < TREE_SPECIES_COUNT; i++) {
        treeModels[i].LoadModel(std::string("models/trees/tree_") + TREE_SPECIES[i] + ".obj");
    }
    loadNearTrees("models/trees/trees_near.txt");
    loadHeightMap("models/terrain/terrain_height.txt");

    // collision shapes, computed once from the meshes
    satuMareBounds = satuMare.GetMeshBounds();
    genexBounds = genex.GetMeshBounds();
    restaurantBounds = genexRestaurant.GetMeshBounds();
    panoramaBounds = panorama.GetMeshBounds();
    telefoaneBounds = telefoane.GetMeshBounds();
    lampPostBounds = lampPost.GetMeshBounds();
    floodlightBounds = floodlight.GetMeshBounds();
    motherlandBounds = motherland.GetMeshBounds();
    romanitaBounds = romanita.GetMeshBounds();
    powerPlantBounds = powerPlant.GetMeshBounds();
    dugaBounds = duga.GetMeshBounds();
    pripyatBounds = pripyatBlocks.GetMeshBounds();
    ferrisSupportBounds = ferrisSupport.GetMeshBounds();
    ferrisWheelBounds = ferrisWheel.GetMeshBounds();
}

void initShaders() {
    screenShader.loadShader(
        "shaders/screen.vert",
        "shaders/screen.frag");
    myBasicShader.loadShader(
        "shaders/basic.vert",
        "shaders/basic.frag");
    rainShader.loadShader(
        "shaders/rain.vert",
        "shaders/rain.frag");
    depthMapShader.loadShader(
        "shaders/shadow.vert",
        "shaders/shadow.frag");
}

void initShadowMap() {
    // depth texture: what the sun "sees" (distance to the closest surface for each texel)
    glGenTextures(1, &depthMapTexture);
    glBindTexture(GL_TEXTURE_2D, depthMapTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE, 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    // outside the shadow map: depth 1 = nothing in front = lit
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float border[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
    glBindTexture(GL_TEXTURE_2D, 0);

    // framebuffer with only a depth attachment, no color
    glGenFramebuffers(1, &depthMapFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMapTexture, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "The shadow map framebuffer is incomplete" << std::endl;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    depthModelLoc = glGetUniformLocation(depthMapShader.shaderProgram, "model");
    depthLightSpaceLoc = glGetUniformLocation(depthMapShader.shaderProgram, "lightSpaceTrMatrix");
    lightSpaceTrMatrixLoc = glGetUniformLocation(myBasicShader.shaderProgram, "lightSpaceTrMatrix");
    shadowMapLoc = glGetUniformLocation(myBasicShader.shaderProgram, "shadowMap");
    shadowsEnabledLoc = glGetUniformLocation(myBasicShader.shaderProgram, "shadowsEnabled");
}

// orthographic projection from the sun (parallel rays), over an area centered on the camera.
// The center is snapped to whole shadow-map texels, so the shadows don't shimmer while the camera moves.
glm::mat4 computeLightSpaceTrMatrix() {
    glm::vec3 sunDir = glm::normalize(lightDir);
    glm::mat4 lightView = glm::lookAt(sunDir, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    glm::vec3 cameraPos = glm::vec3(glm::inverse(view)[3]);
    glm::vec3 center = glm::vec3(lightView * glm::vec4(cameraPos.x, 0.0f, cameraPos.z, 1.0f));
    float texel = SHADOW_AREA / SHADOW_MAP_SIZE;
    center.x = floor(center.x / texel) * texel;
    center.y = floor(center.y / texel) * texel;

    float half = SHADOW_AREA / 2.0f;
    glm::mat4 lightProjection = glm::ortho(center.x - half, center.x + half, center.y - half, center.y + half,
                                           -center.z - SHADOW_DEPTH_RANGE, -center.z + SHADOW_DEPTH_RANGE);
    return lightProjection * lightView;
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
        0.5f, 3000.0f);
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
// frustum culling: the box of the whole model (model space), computed once per model
std::map<const gps::Model3D*, gps::BoundingBox> modelBoxes;

const gps::BoundingBox& modelBox(gps::Model3D& object) {
    auto it = modelBoxes.find(&object);
    if (it != modelBoxes.end()) return it->second;
    gps::BoundingBox box{ glm::vec3(FLT_MAX), glm::vec3(-FLT_MAX) };
    for (const gps::BoundingBox& b : object.GetMeshBounds()) {
        box.min = glm::min(box.min, b.min);
        box.max = glm::max(box.max, b.max);
    }
    return modelBoxes[&object] = box;
}

// Cohen-Sutherland region code (course 05 - Clipping), in 3D: 6 bits, one for each plane of the view volume.
// In clip space a point is inside when -w <= x, y, z <= w.
enum : unsigned { OUT_LEFT = 1, OUT_RIGHT = 2, OUT_BOTTOM = 4, OUT_TOP = 8, OUT_NEAR = 16, OUT_FAR = 32 };

unsigned outCode(const glm::vec4& c) {
    unsigned code = 0;
    if (c.x < -c.w) code |= OUT_LEFT;
    if (c.x > c.w)  code |= OUT_RIGHT;
    if (c.y < -c.w) code |= OUT_BOTTOM;
    if (c.y > c.w)  code |= OUT_TOP;
    if (c.z < -c.w) code |= OUT_NEAR;
    if (c.z > c.w)  code |= OUT_FAR;
    return code;
}

// frustum culling with "trivial reject": if all 8 corners of the box have a bit in common (AND != 0),
// the whole box is beyond the same plane, so the object is surely not visible and is not drawn at all
bool boxVisible(const gps::BoundingBox& box, const glm::mat4& clipMatrix) {
    unsigned common = OUT_LEFT | OUT_RIGHT | OUT_BOTTOM | OUT_TOP | OUT_NEAR | OUT_FAR;
    for (int i = 0; i < 8; i++) {
        glm::vec3 p((i & 1) ? box.max.x : box.min.x, (i & 2) ? box.max.y : box.min.y, (i & 4) ? box.max.z : box.min.z);
        common &= outCode(clipMatrix * glm::vec4(p, 1.0f));
        if (common == 0) return true;   // no common plane: (possibly) visible
    }
    return false;
}

// distance from the camera to the nearest point of the object's box (world space)
float boxDistance(const gps::BoundingBox& box, const glm::mat4& objectModel, const glm::vec3& cameraPos) {
    glm::vec3 lo(FLT_MAX), hi(-FLT_MAX);
    for (int i = 0; i < 8; i++) {
        glm::vec3 p((i & 1) ? box.max.x : box.min.x, (i & 2) ? box.max.y : box.min.y, (i & 4) ? box.max.z : box.min.z);
        glm::vec3 w = glm::vec3(objectModel * glm::vec4(p, 1.0f));
        lo = glm::min(lo, w);
        hi = glm::max(hi, w);
    }
    return glm::distance(cameraPos, glm::clamp(cameraPos, lo, hi));
}

void renderObject(gps::Model3D& object, const glm::mat4& objectModel, gps::Shader shader,
    const glm::vec3& emissive = glm::vec3(0.0f)) {
    // skip the objects that are not in the shadow map area / not in front of the camera
    glm::mat4 clipMatrix = depthPass ? lightSpaceTrMatrix : projection * view;
    if (!boxVisible(modelBox(object), clipMatrix * objectModel)) return;
    if (depthPass) { // shadow map: only the geometry matters
        depthMapShader.useShaderProgram();
        glUniformMatrix4fv(depthModelLoc, 1, GL_FALSE, glm::value_ptr(objectModel));
        object.Draw(depthMapShader);
        return;
    }
    shader.useShaderProgram();
    glUniform3fv(emissiveColorLoc, 1, glm::value_ptr(emissive));
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(objectModel));
    glm::mat3 nm = glm::mat3(glm::inverseTranspose(view * objectModel));
    glUniformMatrix3fv(normalMatrixLoc, 1, GL_FALSE, glm::value_ptr(nm));
    object.Draw(shader);
}

void renderSceneObjects();

void renderScene() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // the shadow map from the depth pass, read by the lighting shader
    myBasicShader.useShaderProgram();
    glUniformMatrix4fv(lightSpaceTrMatrixLoc, 1, GL_FALSE, glm::value_ptr(lightSpaceTrMatrix));
    glUniform1i(shadowsEnabledLoc, shadowsEnabled);
    glActiveTexture(GL_TEXTURE0 + SHADOW_MAP_TEXTURE_UNIT);
    glBindTexture(GL_TEXTURE_2D, depthMapTexture);
    glUniform1i(shadowMapLoc, SHADOW_MAP_TEXTURE_UNIT);
    glActiveTexture(GL_TEXTURE0);

    renderSceneObjects();
}

// pass 1: the scene from the sun, depth only
void renderShadowMap() {
    if (!shadowsEnabled) {
        framesSinceShadowUpdate = SHADOW_UPDATE_INTERVAL;  // redraw it as soon as the shadows are turned back on
        return;
    }
    if (++framesSinceShadowUpdate < SHADOW_UPDATE_INTERVAL) return;  // keep the map (and its matrix) from last time
    framesSinceShadowUpdate = 0;
    lightSpaceTrMatrix = computeLightSpaceTrMatrix();
    depthMapShader.useShaderProgram();
    glUniformMatrix4fv(depthLightSpaceLoc, 1, GL_FALSE, glm::value_ptr(lightSpaceTrMatrix));

    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glViewport(0, 0, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
    glClear(GL_DEPTH_BUFFER_BIT);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);  // wireframe/points don't apply to the shadows
    depthPass = true;
    renderSceneObjects();
    depthPass = false;
    applyViewMode();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, sceneWidth, sceneHeight);
}

// all the objects of the scene; used by both passes (the shadow map and the normal rendering)
// one object to draw in this pass
struct DrawItem {
    gps::Model3D* object;
    glm::mat4 model;
    glm::vec3 emissive;
    float distance;   // from the camera, to the nearest point of the object
};

void renderSceneObjects() {
    std::vector<DrawItem> drawList;
    // brutalist scene (positions in meters)
    glm::mat4 identity(1.0f);
    glm::vec3 cameraPos = glm::vec3(glm::inverse(view)[3]);
    // in the shadow map only the trees and cars near the camera are drawn
    auto castsShadow = [&](const glm::mat4& m) {
        return !depthPass || glm::distance(glm::vec3(m[3]), cameraPos) < SHADOW_DETAIL_DISTANCE;
    };
    // collects the objects; far from the camera the heavy models are replaced by their simplified copies
    auto draw = [&](gps::Model3D& object, const glm::mat4& m, const glm::vec3& emissive = glm::vec3(0.0f)) {
        drawList.push_back({ &object, m, emissive, boxDistance(modelBox(object), m, cameraPos) });
    };
    auto lod = [&](gps::Model3D& full, gps::Model3D& simple, const glm::mat4& m) -> gps::Model3D& {
        return boxDistance(modelBox(full), m, cameraPos) > LOD_DISTANCE ? simple : full;
    };
    draw(terrain, identity);
    draw(trees, identity);   // no colliders: the camera can pass through the foliage
    for (const NearTree& tree : nearTrees) {
        if (!castsShadow(tree.model)) continue;
        draw(treeModels[tree.species], tree.model);
    }
    draw(satuMare, satuMareMatrix());
    draw(genex, genexMatrix());
    draw(genexRestaurant, restaurantMatrix());
    draw(panorama, panoramaMatrix());
    draw(telefoane, telefoaneMatrix());
    draw(lod(motherland, motherlandLod, glm::translate(identity, MOTHERLAND_POS)), glm::translate(identity, MOTHERLAND_POS));
    draw(romanita, glm::translate(identity, ROMANITA_POS));
    draw(lod(powerPlant, powerPlantLod, glm::translate(identity, POWER_PLANT_POS)), glm::translate(identity, POWER_PLANT_POS));
    draw(lod(duga, dugaLod, glm::translate(identity, DUGA_POS)), glm::translate(identity, DUGA_POS));
    draw(lod(pripyatBlocks, pripyatBlocksLod, glm::translate(identity, PRIPYAT_POS)), glm::translate(identity, PRIPYAT_POS));
    draw(ferrisSupport, glm::translate(identity, FERRIS_POS));
    draw(ferrisWheel, ferrisWheelMatrix());
    for (int i = 0; i < FERRIS_GONDOLAS; i++) {
        draw(ferrisGondola, ferrisGondolaMatrix(i));
    }

    // street lamps; the bulbs glow at night
    glm::vec3 bulbGlow = nightMode ? LAMP_GLOW : glm::vec3(0.0f);
    for (const glm::vec3& base : LAMP_POSITIONS) {
        glm::mat4 lampModel = glm::translate(identity, base);
        draw(lampPost, lampModel);
        draw(lampBulb, lampModel, bulbGlow);
    }
    for (const SpotLight& spot : SPOT_LIGHTS) {
        draw(floodlight, glm::translate(identity, spot.position));
        draw(floodlightLens, glm::translate(identity, spot.position), bulbGlow);
    }

    // traffic; the cars face +X, the westbound ones are turned around
    glm::vec3 carLightGlow = nightMode ? glm::vec3(1.0f) : glm::vec3(0.0f);
    if (trafficEnabled) {
        for (const Car& car : cars) {
            glm::mat4 carModel = glm::translate(identity, glm::vec3(car.x, ROAD_Y, LANE_Z[car.lane]));
            if (LANE_DIR[car.lane] < 0.0f) {
                carModel = glm::rotate(carModel, glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            }
            if (!castsShadow(carModel)) continue;
            draw(carModels[car.model], carModel);
            draw(carLights, carModel, carLightGlow);
        }
    }
    // z-buffer (course 07 - Fragment processing): drawn from near to far, the hidden fragments of the objects behind
    // fail the depth test before the (expensive) lighting shader runs for them; the order doesn't matter for the depth map
    if (!depthPass) {
        std::sort(drawList.begin(), drawList.end(),
            [](const DrawItem& a, const DrawItem& b) { return a.distance < b.distance; });
    }
    for (const DrawItem& item : drawList) {
        renderObject(*item.object, item.model, myBasicShader, item.emissive);
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

// draws the resolved scene over the whole window
void renderScreenQuad() {
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); // the wireframe/points view modes apply to the scene, not to this copy
    glDisable(GL_DEPTH_TEST);
    screenShader.useShaderProgram();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, resolveTexture);
    glUniform1i(glGetUniformLocation(screenShader.shaderProgram, "screenTexture"), 0);
    glBindVertexArray(screenVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
    applyViewMode();
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

        ImGui::SeparatorText("Traffic");
        ImGui::Checkbox("Traffic", &trafficEnabled);
        ImGui::SameLine();
        ImGui::TextDisabled("(%d cars on the road)", (int)cars.size());

        ImGui::SeparatorText("Rendering");
        ImGui::Checkbox("Shadows", &shadowsEnabled);
        if (ImGui::Checkbox("Antialiasing (MSAA 4x)", &antialiasingEnabled)) {
            createSceneFramebuffers(sceneWidth, sceneHeight);
        }

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
            keyRow("O", "Shadows on / off");
            keyRow("B", "Antialiasing on / off");
            keyRow("V", "Traffic on / off");
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
    glDeleteFramebuffers(1, &depthMapFBO);
    glDeleteTextures(1, &depthMapTexture);
    deleteSceneFramebuffers();
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
    initShadowMap();
    glGenVertexArrays(1, &screenVAO);
    createSceneFramebuffers(myWindow.getWindowDimensions().width, myWindow.getWindowDimensions().height);
    initTraffic();
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
        updateTraffic();
        // 0. shadow map: the scene from the sun
        renderShadowMap();
        // 1. the scene (and the rain) into the multisampled framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, msaaFBO);
        renderScene();
        renderRain();
        // 2. resolve: average the samples of each pixel into the texture
        glBindFramebuffer(GL_READ_FRAMEBUFFER, msaaFBO);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolveFBO);
        glBlitFramebuffer(0, 0, sceneWidth, sceneHeight, 0, 0, sceneWidth, sceneHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        // 3. copy the texture to the window, then the UI on top
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        renderScreenQuad();
        renderUI();

        glfwPollEvents();
        glfwSwapBuffers(myWindow.getWindow());

        glCheckError();
    }

    cleanup();

    return EXIT_SUCCESS;
}