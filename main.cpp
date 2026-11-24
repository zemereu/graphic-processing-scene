// Graphics processing project: a brutalist / Soviet-era city and landscape (OpenGL 4.1).
// This file only creates the window, initializes the modules and runs the frame loop; the modules are:
//   App        shared state: window, camera, matrices, frame time, keys, main shader and its uniforms
//   Scene      the models, where they stand, the moving objects, the town's props and the trees
//   Lighting   time of day: sun / moon, street lamps, floodlights, fog
//   Shadows    the shadow map
//   Skybox     the sky around the scene (also the color of the fog and what the water reflects)
//   Renderer   view modes, antialiasing framebuffer, frustum culling, level of detail, the draw list
//   Rain       the rain
//   Collision  camera collisions with the buildings and the ground
//   Traffic    cars driving on all the roads
//   Tour       the camera tour
//   Input      keyboard and mouse
//   UI         the control panel
#include "App.hpp"
#include "Scene.hpp"
#include "Lighting.hpp"
#include "Shadows.hpp"
#include "Skybox.hpp"
#include "Renderer.hpp"
#include "Rain.hpp"
#include "Collision.hpp"
#include "Traffic.hpp"
#include "Tour.hpp"
#include "Input.hpp"
#include "UI.hpp"

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
    initModels();
    initColliders();
    loadTraffic("models/city/traffic.txt");
    initShaders();
    initUniforms();
    initRain();
    initShadowMap();
    initSkybox();
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
        updateDayCycle();   // the sun moves; the town's lights come on / go off
        updateWeather();    // the ground gets wet in the rain (dries after), lightning
        updateTraffic();
        // 0. shadow maps: the scene from the sun / moon (and from the floodlights at night)
        renderShadowMap();
        renderSpotShadowMaps();   // at night: the floodlights' shadow maps
        // 1. the scene (and the rain) into the multisampled framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, msaaFBO);
        renderScene();
        renderSkybox();   // after the objects: only fills the pixels they left empty
        renderRain();
        // 2. resolve: average the samples of each pixel into the texture
        glBindFramebuffer(GL_READ_FRAMEBUFFER, msaaFBO);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolveFBO);
        glBlitFramebuffer(0, 0, sceneWidth, sceneHeight, 0, 0, sceneWidth, sceneHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        // 3. the glow of the brightest parts, then the image to the window (tone mapped), then the UI on top
        renderBloom();
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
