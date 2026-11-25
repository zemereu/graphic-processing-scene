#include "Controls.hpp"
#include "Scene.hpp"
#include "Lighting.hpp"
#include "Renderer.hpp"
#include "Environment.hpp"
#include "PostProcess.hpp"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#include <cfloat>
#include <fstream>
#include <cmath>

// ==============================================================================================================
// Input
// ==============================================================================================================

float cameraSpeed = 30.0f;
float cameraSprintFactor = 4.0f;

// mouse
bool firstMouse = true;
double lastX, lastY;
float yaw = -90.0f, pitch = 0.0f;
float mouseSensitivity = 0.1f;

const float OBJECT_MOVE_SPEED = 20.0f;    // meters per second
const float OBJECT_ROTATE_SPEED = 45.0f;  // degrees per second
const float OBJECT_SCALE_SPEED = 0.5f;    // +50% per second

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
        if (key == GLFW_KEY_N) toggleDayNight();
        if (key == GLFW_KEY_L) dayCycleRunning = !dayCycleRunning;
        if (key == GLFW_KEY_F) { fogEnabled = !fogEnabled; applyFog(); }
        if (key == GLFW_KEY_P) setRain(!rainEnabled);
        if (key == GLFW_KEY_K) setSnow(!snowEnabled);
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
    uploadView();
}

void setWindowCallbacks() {
    glfwSetWindowSizeCallback(myWindow.getWindow(), windowResizeCallback);
    glfwSetKeyCallback(myWindow.getWindow(), keyboardCallback);
    glfwSetCursorPosCallback(myWindow.getWindow(), mouseCallback);
    glfwSetInputMode(myWindow.getWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
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
        uploadView();
    }

    if (pressedKeys[GLFW_KEY_S]) {
        moveCamera(gps::MOVE_BACKWARD, distance);
        uploadView();
    }

    if (pressedKeys[GLFW_KEY_A]) {
        moveCamera(gps::MOVE_LEFT, distance);
        uploadView();
    }

    if (pressedKeys[GLFW_KEY_D]) {
        moveCamera(gps::MOVE_RIGHT, distance);
        uploadView();
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

// ==============================================================================================================
// Collision
// ==============================================================================================================

// collision detection: the camera is a sphere that can't enter the buildings' boxes
const float CAMERA_RADIUS = 1.5f;      // meters
const float CAMERA_MIN_HEIGHT = 1.8f;  // eye height above the ground (terrain height map)
bool collisionsEnabled = true;
std::vector<gps::BoundingBox> satuMareBounds, genexBounds, restaurantBounds, panoramaBounds,
                              telefoaneBounds, lampPostBounds, floodlightBounds; // model space, from the meshes
std::vector<gps::BoundingBox> worldColliders; // world space, rebuilt every frame (some objects move)
std::vector<gps::BoundingBox> motherlandBounds, romanitaBounds, powerPlantBounds, dugaBounds, pripyatBounds,
                              ferrisSupportBounds, ferrisWheelBounds;
std::vector<gps::BoundingBox> cityNorthBounds, citySouthBounds;

// terrain height map (written by tools/gen_world.py together with the terrain),
// used to keep the camera above the ground: heights on a regular grid, read once at startup
struct HeightMap {
    float x0 = 0.0f, z0 = 0.0f, step = 1.0f;   // world position of the first sample, grid spacing
    int nx = 0, nz = 0;
    std::vector<float> heights;                // nz rows of nx values
};
HeightMap terrainHeights;

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

void initColliders() {
    cityNorthBounds = cityNorth.GetMeshBounds();
    citySouthBounds = citySouth.GetMeshBounds();
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
    addColliders(cityNorthBounds, glm::mat4(1.0f));
    addColliders(citySouthBounds, glm::mat4(1.0f));
    for (const glm::vec3& base : LAMP_POSITIONS) {
        addColliders(lampPostBounds, glm::translate(glm::mat4(1.0f), base));
    }
    for (const SpotLight& spot : SPOT_LIGHTS) {
        addColliders(floodlightBounds, glm::translate(glm::mat4(1.0f), spot.position));
    }
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

// ==============================================================================================================
// Tour
// ==============================================================================================================

// camera tour: the camera follows a smooth path through these key points
struct TourKey {
    glm::vec3 position;
    glm::vec3 target;   // point the camera looks at
};
const std::vector<TourKey> TOUR = {
    {{100.0f, 40.0f, 990.0f}, {100.0f, 8.0f, 700.0f}},  // from the road coming in from the south, the roundabout ahead
    {{100.0f, 16.0f, 738.0f}, {100.0f, 12.0f, 660.0f}},  // the monument in the middle of the roundabout
    {{152.0f, 15.0f, 684.0f}, {100.0f, 14.0f, 660.0f}},  // around the roundabout: the curved streets
    {{100.0f, 14.0f, 612.0f}, {100.0f, 12.0f, 300.0f}},  // down the boulevard
    {{128.0f, 20.0f, 330.0f}, {200.0f, 40.0f, 280.0f}},  // Romanita in its park
    {{132.0f, 9.0f, 214.0f}, {0.0f, 35.0f, 70.0f}},  // the crossing with the highway, the Administrative Palace
    {{-30.0f, 6.0f, 150.0f}, {0.0f, 60.0f, 60.0f}},  // look up at the palace from its plaza
    {{92.0f, 10.0f, 118.0f}, {70.0f, 6.0f, 30.0f}},  // along the boulevard, past the palace
    {{62.0f, 9.0f, 24.0f}, {-60.0f, 3.0f, -40.0f}},  // on the quay: the river, the footbridge
    {{-100.0f, 10.0f, 8.0f}, {-170.0f, 10.0f, 140.0f}},  // along the promenade
    {{-118.0f, 15.0f, 214.0f}, {-170.0f, 10.0f, 140.0f}},  // Palatul Telefoanelor
    {{-300.0f, 10.0f, 196.0f}, {-420.0f, 40.0f, 196.0f}},  // west along the highway, the Genex gate
    {{-470.0f, 7.0f, 196.0f}, {-640.0f, 10.0f, 230.0f}},  // the checkpoint, into the exclusion zone
    {{-590.0f, 30.0f, 390.0f}, {-690.0f, 15.0f, 300.0f}},  // Pripyat and the Ferris wheel
    {{-650.0f, 60.0f, 190.0f}, {-780.0f, 30.0f, 70.0f}},  // the power plant
    {{-680.0f, 75.0f, -150.0f}, {-700.0f, 60.0f, -330.0f}},  // over the river, the Duga radar
    {{-300.0f, 110.0f, -330.0f}, {-100.0f, 80.0f, -520.0f}},  // along the north bank, Hotel Panorama in the mountains
    {{260.0f, 75.0f, -70.0f}, {450.0f, 60.0f, -150.0f}},  // Motherland Calls on her hill
    {{560.0f, 70.0f, 40.0f}, {520.0f, 20.0f, 400.0f}},  // back over the river, down the curved avenue in the east
    {{610.0f, 50.0f, 420.0f}, {470.0f, 10.0f, 640.0f}},  // over the eastern housing estate
    {{420.0f, 42.0f, 900.0f}, {100.0f, 15.0f, 700.0f}},  // the new southern district, back towards the roundabout
    {{100.0f, 40.0f, 990.0f}, {100.0f, 8.0f, 700.0f}},  // back to the start
};
const float TOUR_SPEED = 35.0f;       // meters per second along the path
const float TOUR_MIN_SEGMENT = 3.0f;  // seconds, so short segments are not rushed
std::vector<float> tourKeyTimes;      // time at which the camera reaches each key point
bool tourActive = false;
float tourTime = 0.0f;

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

// ==============================================================================================================
// UI
// ==============================================================================================================

bool showUI = true;

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

        ImGui::SeparatorText("Time of day");
        int hours = (int)timeOfDay, minutes = (int)((timeOfDay - hours) * 60.0f);
        char clock[8];
        snprintf(clock, sizeof(clock), "%02d:%02d", hours, minutes);   // shown on the slider instead of the number
        ImGui::SliderFloat("Time", &timeOfDay, 0.0f, 23.99f, clock);
        ImGui::Checkbox("Animate", &dayCycleRunning);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        ImGui::SliderFloat("Speed", &dayCycleSpeed, 0.02f, 1.0f, "%.2f h/s");

        ImGui::SeparatorText("Fog");
        if (ImGui::Checkbox("Fog", &fogEnabled)) applyFog();
        if (ImGui::SliderFloat("Density", &fogDensity, 0.0005f, 0.01f, "%.4f")) applyFog();

        ImGui::SeparatorText("Weather");
        bool rain = rainEnabled, snow = snowEnabled;
        if (ImGui::Checkbox("Rain", &rain)) setRain(rain);
        ImGui::SameLine();
        if (ImGui::Checkbox("Snow", &snow)) setSnow(snow);
        ImGui::SameLine();
        ImGui::Checkbox("Lightning", &lightningEnabled);
        ImGui::SliderFloat("Wind speed", &windSpeed, 0.0f, 20.0f, "%.0f m/s");
        ImGui::SliderFloat("Wind direction", &windDirection, 0.0f, 360.0f, "%.0f deg");
        ImGui::Checkbox("Power plant smoke", &smokeEnabled);
        ImGui::SameLine();
        ImGui::Checkbox("Birds", &birdsEnabled);
        ImGui::SameLine();
        ImGui::Checkbox("Grass", &grassEnabled);
        ImGui::SameLine();
        ImGui::Checkbox("Flowers", &flowersEnabled);

        ImGui::SeparatorText("Traffic");
        ImGui::Checkbox("Traffic", &trafficEnabled);
        ImGui::SameLine();
        ImGui::TextDisabled("(%d cars on the roads)", (int)cars.size());
        if (ImGui::SliderInt("Cars", &trafficCars, 0, 400)) initTraffic();

        ImGui::SeparatorText("Rendering");
        ImGui::Checkbox("Shadows", &shadowsEnabled);
        ImGui::SameLine();
        ImGui::Checkbox("Show shadow cascades", &showShadowMap);
        ImGui::Checkbox("Bloom", &bloomEnabled);
        ImGui::SameLine();
        ImGui::Checkbox("God rays", &godRaysEnabled);
        ImGui::SameLine();
        ImGui::Checkbox("SSAO", &ssaoEnabled);
        ImGui::Checkbox("Water reflections", &reflectionsEnabled);
        ImGui::SameLine();
        ImGui::Checkbox("Light beams", &lightBeamsEnabled);
        ImGui::Checkbox("Auto exposure", &autoExposureEnabled);
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
            keyRow("N", "Jump to day / night");
            keyRow("L", "Day cycle on / off");
            keyRow("F", "Fog on / off");
            keyRow("P", "Rain on / off");
            keyRow("K", "Snow on / off");
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

    // the shadow cascades: the scene seen from the sun / moon (dark = close to the light), near to far
    if (showUI && showShadowMap && shadowsEnabled) {
        const float size = 180.0f;
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 10.0f, io.DisplaySize.y - 10.0f), ImGuiCond_Always,
            ImVec2(1.0f, 1.0f));
        ImGui::Begin("Shadow cascades (view from the light)", &showShadowMap,
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
        for (int i = 0; i < SHADOW_CASCADES; i++) {
            if (i > 0) ImGui::SameLine();
            ImGui::BeginGroup();
            ImGui::Text("%.0f - %.0f m", CASCADE_SPLITS[i], CASCADE_SPLITS[i + 1]);
            // flipped vertically: OpenGL textures start at the bottom, ImGui at the top
            ImGui::Image((ImTextureID)shadowPreviewTexture[i], ImVec2(size, size), ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
            ImGui::EndGroup();
        }
        ImGui::End();
    }

    ImGui::Render();
    // ImGui colors are already sRGB: drawing them through the sRGB framebuffer would wash them out
    glDisable(GL_FRAMEBUFFER_SRGB);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glEnable(GL_FRAMEBUFFER_SRGB);
}

void shutdownUI() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}
