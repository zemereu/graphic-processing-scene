// Graphics processing project: a brutalist / Soviet-era city and landscape (OpenGL 4.1).
// This file only creates the window, initializes the modules and runs the frame loop; the modules are:
//   App          shared state: window, camera, matrices, frame time, keys, main shader and its uniforms
//   Scene        the models, where they stand, the moving objects, the town's props, the trees, the traffic
//   Lighting     time of day (sun / moon, street lamps, floodlights, fog), the skybox, the shadow maps
//   Environment  rain / snow / wind / lightning, 3D grass and flowers, smoke from the power plant, birds (boids),
//                the sounds
//   Renderer     view modes, antialiasing framebuffer, culling, level of detail, the draw list, the passes,
//                the water reflections
//   PostProcess  SSAO, god rays, light beams, bloom, auto exposure, tone mapping
//   Controls     keyboard and mouse, camera collisions, the camera tour, the control panel (ImGui)
#include "App.hpp"
#include "Scene.hpp"
#include "Lighting.hpp"
#include "Renderer.hpp"
#include "PostProcess.hpp"
#include "Environment.hpp"
#include "Controls.hpp"

#ifdef _WIN32
// on laptops with two GPUs, ask the driver to run this program on the dedicated one
extern "C" {
    __declspec(dllexport) unsigned long NvOptimusEnablement = 1;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

void initOpenGLWindow() {
    myWindow.Create(1280, 720, "Proiect");
}

void cleanup() {
    glDeleteFramebuffers(1, &depthMapFBO);
    glDeleteTextures(1, &depthMapTexture);
    deleteSceneFramebuffers();
    shutdownSound();
    shutdownUI();
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
    setWindowCallbacks();
    initUI();          // first: it draws the loading screen
    initModels();      // the loading screen's bar: 0 - 85%
    showLoading(0.86f, "collisions, traffic");
    initColliders();
    loadTraffic("models/city/traffic.txt");
    loadTrolleyRoute("models/trolleybus/route.txt");
    showLoading(0.88f, "shaders");
    initShaders();
    initPostProcess();
    initReflection();
    initUniforms();
    showLoading(0.92f, "rain, smoke, birds, shadows, sky");
    initRain();
    initSmoke();
    initBirds();
    initShadowMap();
    initSkybox();
    glGenVertexArrays(1, &screenVAO);
    createSceneFramebuffers(myWindow.getWindowDimensions().width, myWindow.getWindowDimensions().height);
    showLoading(0.96f, "grass");
    initGrass();   // draws the grass map: needs the scene's size (viewport) and the shaders
    initTraffic();
    initTour();
    initSound();
    showLoading(1.0f, "ready");
    sceneReady = true;

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
        updateDayCycle();   // the sun moves; the town's lights come on / go off
        updateWeather();    // the ground gets wet in the rain (dries after), lightning
        updateTraffic();
        updateBirds();
        updatePedestrians();
        updateSound();
        renderFrame();   // shadow maps, scene, post-processing (Renderer.cpp)
        renderUI();

        glfwPollEvents();
        glfwSwapBuffers(myWindow.getWindow());

        glCheckError();
    }

    cleanup();

    return EXIT_SUCCESS;
}
