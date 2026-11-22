#include "UI.hpp"
#include "Scene.hpp"
#include "Lighting.hpp"
#include "Renderer.hpp"
#include "Shadows.hpp"
#include "Rain.hpp"
#include "Collision.hpp"
#include "Traffic.hpp"
#include "Tour.hpp"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#include <cmath>

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
        ImGui::TextDisabled("(%d cars on the roads)", (int)cars.size());
        if (ImGui::SliderInt("Cars", &trafficCars, 0, 400)) initTraffic();

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

void shutdownUI() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}
