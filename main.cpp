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
gps::Model3D lampPost;
gps::Model3D lampBulb;

// UI
bool showUI = true;

// shaders
gps::Shader myBasicShader;

GLenum glCheckError_(const char* file, int line){
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

void applyLighting() {
    myBasicShader.useShaderProgram();
    lightColor = nightMode ? MOON_COLOR : SUN_COLOR;
    glUniform3fv(lightColorLoc, 1, glm::value_ptr(lightColor));
    glUniform1i(numPointLightsLoc, nightMode ? (GLint)LAMP_POSITIONS.size() : 0);
    if (nightMode) glClearColor(0.02f, 0.03f, 0.06f, 1.0f);
    else glClearColor(0.7f, 0.7f, 0.7f, 1.0f);
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
    float distance = cameraSpeed * deltaTime;
    if (pressedKeys[GLFW_KEY_LEFT_SHIFT]) {
        distance *= cameraSprintFactor;
    }

    if (pressedKeys[GLFW_KEY_W]) {
        myCamera.move(gps::MOVE_FORWARD, distance);
        //update view matrix
        view = myCamera.getViewMatrix();
        myBasicShader.useShaderProgram();
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    }

    if (pressedKeys[GLFW_KEY_S]) {
        myCamera.move(gps::MOVE_BACKWARD, distance);
        //update view matrix
        view = myCamera.getViewMatrix();
        myBasicShader.useShaderProgram();
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    }

    if (pressedKeys[GLFW_KEY_A]) {
        myCamera.move(gps::MOVE_LEFT, distance);
        //update view matrix
        view = myCamera.getViewMatrix();
        myBasicShader.useShaderProgram();
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    }

    if (pressedKeys[GLFW_KEY_D]) {
        myCamera.move(gps::MOVE_RIGHT, distance);
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
}

void initShaders() {
    myBasicShader.loadShader(
        "shaders/basic.vert",
        "shaders/basic.frag");
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

    flatShadingLoc = glGetUniformLocation(myBasicShader.shaderProgram, "flatShading");
    glUniform1i(flatShadingLoc, flatShading);

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
    renderObject(satuMare, glm::translate(identity, glm::vec3(0.0f, 0.0f, 60.0f)), myBasicShader);
    renderObject(genex, glm::translate(identity, glm::vec3(260.0f, 0.0f, 196.0f)), myBasicShader);
    // restaurant: rotate around its own axis first, then move it on top of the tower (T * R)
    glm::mat4 restaurantModel = glm::translate(identity, glm::vec3(260.0f, 108.0f, 229.0f));
    restaurantModel = glm::rotate(restaurantModel, glm::radians(restaurantAngle), glm::vec3(0.0f, 1.0f, 0.0f));
    renderObject(genexRestaurant, restaurantModel, myBasicShader);
    renderObject(panorama, glm::translate(identity, glm::vec3(-60.0f, 40.0f, -260.0f)), myBasicShader);
    // T * R * S: scale and rotate around the building's own base, then move it into place
    glm::mat4 telefoaneModel = glm::translate(identity, telefoanePos);
    telefoaneModel = glm::rotate(telefoaneModel, glm::radians(telefoaneAngle), glm::vec3(0.0f, 1.0f, 0.0f));
    telefoaneModel = glm::scale(telefoaneModel, glm::vec3(telefoaneScale));
    renderObject(telefoane, telefoaneModel, myBasicShader);

    // street lamps; the bulbs glow at night
    glm::vec3 bulbGlow = nightMode ? LAMP_GLOW : glm::vec3(0.0f);
    for (const glm::vec3& base : LAMP_POSITIONS) {
        glm::mat4 lampModel = glm::translate(identity, base);
        renderObject(lampPost, lampModel, myBasicShader);
        renderObject(lampBulb, lampModel, myBasicShader, bulbGlow);
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
        if (ImGui::Checkbox("Night (street lamps on)", &nightMode)) applyLighting();

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
    setWindowCallbacks();
    initUI();

    glCheckError();
    // application loop
    lastFrameTime = glfwGetTime();
    while (!glfwWindowShouldClose(myWindow.getWindow())) {
        double now = glfwGetTime();
        deltaTime = (float)(now - lastFrameTime);
        lastFrameTime = now;

        processMovement();
        processObjectTransform();
        updateAnimations();
        renderScene();
        renderUI();

        glfwPollEvents();
        glfwSwapBuffers(myWindow.getWindow());

        glCheckError();
    }

    cleanup();

    return EXIT_SUCCESS;
}