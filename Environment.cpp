#include "Environment.hpp"
#include "Lighting.hpp"
#include "Renderer.hpp"
#include "stb_image.h"
#include "Scene.hpp"
#include "Controls.hpp"

#include <random>

// ==============================================================================================================
// Rain
// ==============================================================================================================

const int RAIN_DROPS = 20000;
const float RAIN_FALL_SPEED = 25.0f;   // meters per second
const float RAIN_AREA_SIZE = 80.0f;    // the rain box around the camera, meters
const float RAIN_AREA_HEIGHT = 40.0f;
const float RAIN_DROP_LENGTH = 0.8f;
bool rainEnabled = false;
bool snowEnabled = false;
float wetness = 0.0f;
float snowCover = 0.0f;
GLint wetnessLoc, snowCoverLoc;   // in myBasicShader
GLint rainSnowLoc, rainFragSnowLoc;
bool lightningEnabled = true;
glm::vec3 flashDirection(0.0f, 1.0f, 0.0f);
glm::vec3 flashColor(0.0f);
float nextStrike = 3.0f;         // seconds until the next lightning
float sinceStrike = 1000.0f;     // seconds since the last one
std::mt19937 lightningRng(7);
float windSpeed = 6.0f;        // meters per second
float windDirection = 30.0f;   // degrees, 0 = blowing towards +X
gps::Shader rainShader;
GLuint rainVAO, rainVBO;
GLint rainViewLoc, rainProjectionLoc, rainCameraPosLoc, rainTimeLoc, rainFallSpeedLoc, rainWindLoc;
GLint rainAreaSizeLoc, rainAreaHeightLoc, rainDropLengthLoc, rainColorLoc;

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
    rainSnowLoc = glGetUniformLocation(program, "snow");
    wetnessLoc = glGetUniformLocation(myBasicShader.shaderProgram, "wetness");
    snowCoverLoc = glGetUniformLocation(myBasicShader.shaderProgram, "snowCover");
}

// brightness of a flash t seconds after the strike: three quick flickers, each fading fast
float flashIntensity(float t) {
    float f = exp(-t / 0.05f);
    if (t > 0.12f) f += 0.6f * exp(-(t - 0.12f) / 0.06f);
    if (t > 0.30f) f += 0.8f * exp(-(t - 0.30f) / 0.10f);
    return t < 1.5f ? f : 0.0f;
}

void updateWeather() {
    // wet ground
    if (rainEnabled) {
        wetness = glm::min(wetness + deltaTime / WETTING_TIME, 1.0f);
    } else {
        wetness = glm::max(wetness - deltaTime / DRYING_TIME, 0.0f);
    }
    // snow cover
    if (snowEnabled) {
        snowCover = glm::min(snowCover + deltaTime / SNOW_COVER_TIME, 1.0f);
    } else {
        snowCover = glm::max(snowCover - deltaTime / SNOW_MELT_TIME, 0.0f);
    }
    myBasicShader.useShaderProgram();
    glUniform1f(wetnessLoc, wetness);
    glUniform1f(snowCoverLoc, snowCover);

    // lightning: a random delay until the next strike, then a random spot in the sky
    std::uniform_real_distribution<float> random01(0.0f, 1.0f);
    sinceStrike += deltaTime;
    if (rainEnabled && lightningEnabled) {
        nextStrike -= deltaTime;
        if (nextStrike <= 0.0f) {
            nextStrike = glm::mix(LIGHTNING_MIN_DELAY, LIGHTNING_MAX_DELAY, random01(lightningRng));
            sinceStrike = 0.0f;
            float azimuth = glm::two_pi<float>() * random01(lightningRng);
            float elevation = glm::radians(glm::mix(25.0f, 60.0f, random01(lightningRng)));
            flashDirection = glm::vec3(cos(elevation) * cos(azimuth), sin(elevation), cos(elevation) * sin(azimuth));
        }
    }
    flashColor = LIGHTNING_COLOR * flashIntensity(sinceStrike);
}

void setRain(bool on) {
    rainEnabled = on;
    if (on) snowEnabled = false;
}

void setSnow(bool on) {
    snowEnabled = on;
    if (on) rainEnabled = false;
}

void renderRain() {
    if (!rainEnabled && !snowEnabled) return;
    bool snow = snowEnabled;

    rainShader.useShaderProgram();
    glm::vec3 cameraPos = glm::vec3(glm::inverse(view)[3]); // camera position = translation of the inverse view
    glm::vec2 wind = windSpeed * glm::vec2(cos(glm::radians(windDirection)), sin(glm::radians(windDirection)));
    // drops reflect the sky: lighter by day, dim at night
    glm::vec4 color = nightMode ? glm::vec4(0.45f, 0.5f, 0.6f, 0.35f) : glm::vec4(0.8f, 0.85f, 0.9f, 0.45f);
    if (snow) {   // white flakes, lit by the day / the street lamps at night
        color = nightMode ? glm::vec4(0.55f, 0.57f, 0.62f, 0.8f) : glm::vec4(0.95f, 0.96f, 1.0f, 0.9f);
    }

    glUniformMatrix4fv(rainViewLoc, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(rainProjectionLoc, 1, GL_FALSE, glm::value_ptr(projection));
    glUniform3fv(rainCameraPosLoc, 1, glm::value_ptr(cameraPos));
    glUniform1f(rainTimeLoc, (float)glfwGetTime());
    glUniform1f(rainFallSpeedLoc, snow ? SNOW_FALL_SPEED : RAIN_FALL_SPEED);
    glUniform1i(rainSnowLoc, snow);
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
    if (snow) {
        glEnable(GL_PROGRAM_POINT_SIZE);   // the vertex shader sets the size of each flake
        glDrawArrays(GL_POINTS, 0, RAIN_DROPS * 2);
        glDisable(GL_PROGRAM_POINT_SIZE);
    } else {
        glDrawArrays(GL_LINES, 0, RAIN_DROPS * 2);
    }
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

// ==============================================================================================================
// Grass
// ==============================================================================================================

bool grassEnabled = true;
bool flowersEnabled = true;
bool grassMaskPass = false;
gps::Shader grassShader, grassMaskShader, tuftShader;
GLuint grassMapFBO = 0, grassMapTexture = 0, grassMapDepthRBO = 0;
GLuint grassVAO = 0, grassVBO = 0;
GLuint tuftVAO = 0, tuftVBO = 0, tuftTexture = 0;
const int GRASS_MAP_TEXTURE_UNIT = 10;

// world -> grass map: a top view of the whole world. x and z map to the image, the height to the depth
// (the higher, the nearer: the depth test keeps the highest surface)
glm::mat4 grassMapMatrix() {
    glm::mat4 m(0.0f);
    m[0][0] = 1.0f / WORLD_HALF_SIZE;   // x -> horizontal
    m[2][1] = 1.0f / WORLD_HALF_SIZE;   // z -> vertical
    m[1][2] = -1.0f / 300.0f;           // y from -50 to 550 m -> depth from 1 to -1
    m[3][2] = 1.0f - 50.0f / 300.0f;
    m[3][3] = 1.0f;
    return m;
}

void initGrass() {
    grassShader.loadShader("shaders/grass.vert", "shaders/grass.frag");
    grassMaskShader.loadShader("shaders/grass_map.vert", "shaders/grass_map.frag");
    tuftShader.loadShader("shaders/grass_tuft.vert", "shaders/grass_tuft.frag");

    // the grass map: red = is this grass ground, green = height of the highest surface (meters)
    glGenTextures(1, &grassMapTexture);
    glBindTexture(GL_TEXTURE_2D, grassMapTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, GRASS_MAP_SIZE, GRASS_MAP_SIZE, 0, GL_RG, GL_FLOAT, nullptr);
    // nearest: blending a grass texel with a building's roof would lift blades into the air at its edge
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    glGenRenderbuffers(1, &grassMapDepthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, grassMapDepthRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, GRASS_MAP_SIZE, GRASS_MAP_SIZE);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glGenFramebuffers(1, &grassMapFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, grassMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, grassMapTexture, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, grassMapDepthRBO);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "The grass map framebuffer is incomplete" << std::endl;
    }

    // draw the whole scene once from above (the objects are static enough: the grass map is made only once)
    glViewport(0, 0, GRASS_MAP_SIZE, GRASS_MAP_SIZE);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_CULL_FACE);   // from above, some surfaces are seen from their back
    grassMaskShader.useShaderProgram();
    glUniformMatrix4fv(glGetUniformLocation(grassMaskShader.shaderProgram, "mapMatrix"), 1, GL_FALSE,
        glm::value_ptr(grassMapMatrix()));
    depthPass = true;
    grassMaskPass = true;
    depthPassMatrix = grassMapMatrix();
    renderSceneObjects();
    grassMaskPass = false;
    depthPass = false;
    glEnable(GL_CULL_FACE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, sceneWidth, sceneHeight);
    applyLighting();   // restores the clear color

    // one blade: a strip narrowing to a point; x = across the blade (-1..1), y = along it (0 = root, 1 = tip)
    const float blade[] = { -1.0f, 0.0f,  1.0f, 0.0f,  -0.8f, 0.4f,  0.8f, 0.4f,  -0.5f, 0.75f,  0.5f, 0.75f,  0.0f, 1.0f };
    glGenVertexArrays(1, &grassVAO);
    glBindVertexArray(grassVAO);
    glGenBuffers(1, &grassVBO);
    glBindBuffer(GL_ARRAY_BUFFER, grassVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(blade), blade, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    // one tuft: two crossed quads (x = across -1..1, y = up 0..1, z = which quad)
    float tuft[2 * 6 * 3];
    const float corners[6][2] = { {-1, 0}, {1, 0}, {1, 1}, {-1, 0}, {1, 1}, {-1, 1} };
    for (int q = 0; q < 2; q++) {
        for (int k = 0; k < 6; k++) {
            tuft[(q * 6 + k) * 3 + 0] = corners[k][0];
            tuft[(q * 6 + k) * 3 + 1] = corners[k][1];
            tuft[(q * 6 + k) * 3 + 2] = (float)q;
        }
    }
    glGenVertexArrays(1, &tuftVAO);
    glBindVertexArray(tuftVAO);
    glGenBuffers(1, &tuftVBO);
    glBindBuffer(GL_ARRAY_BUFFER, tuftVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(tuft), tuft, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    // the pictures of the tufts, side by side in one texture
    int width, height, channels;
    stbi_set_flip_vertically_on_load(true);   // OpenGL expects the first row at the bottom
    unsigned char* data = stbi_load("models/grass/tufts.png", &width, &height, &channels, 4);
    stbi_set_flip_vertically_on_load(false);
    if (!data) {
        std::cerr << "Could not load models/grass/tufts.png" << std::endl;
        return;
    }
    glGenTextures(1, &tuftTexture);
    glBindTexture(GL_TEXTURE_2D, tuftTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(data);
}

// the uniforms the blades and the tufts share: camera, wind, snow, the grass map, the lights and the shadows
static void setGrassUniforms(GLuint program, int grid, float spacing) {
    glm::vec3 cameraPos = glm::vec3(glm::inverse(view)[3]);
    glUniformMatrix4fv(glGetUniformLocation(program, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(program, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniform3fv(glGetUniformLocation(program, "cameraPos"), 1, glm::value_ptr(cameraPos));
    glUniform1f(glGetUniformLocation(program, "time"), (float)glfwGetTime());
    glm::vec2 wind = windSpeed * glm::vec2(cos(glm::radians(windDirection)), sin(glm::radians(windDirection)));
    glUniform2fv(glGetUniformLocation(program, "wind"), 1, glm::value_ptr(wind));
    glUniform1i(glGetUniformLocation(program, "grid"), grid);
    glUniform1f(glGetUniformLocation(program, "spacing"), spacing);
    glUniform1f(glGetUniformLocation(program, "worldHalfSize"), WORLD_HALF_SIZE);
    glUniform1f(glGetUniformLocation(program, "snowCover"), snowCover);
    // light: the sun / moon, the sky (ambient, darker at night), the street lamps
    glUniform3fv(glGetUniformLocation(program, "lightDir"), 1, glm::value_ptr(glm::normalize(lightDir)));
    glUniform3fv(glGetUniformLocation(program, "lightColor"), 1, glm::value_ptr(lightColor));
    glm::vec3 ambient = glm::mix(glm::vec3(0.02f, 0.02f, 0.035f), glm::vec3(0.11f, 0.13f, 0.17f), dayFactor);
    glUniform3fv(glGetUniformLocation(program, "ambientColor"), 1, glm::value_ptr(ambient));
    std::vector<glm::vec3> lamps;
    for (const glm::vec3& base : LAMP_POSITIONS) lamps.push_back(base + glm::vec3(0.0f, LAMP_LIGHT_HEIGHT, 0.0f));
    glUniform1i(glGetUniformLocation(program, "numPointLights"), nightMode ? (GLint)lamps.size() : 0);
    glUniform3fv(glGetUniformLocation(program, "pointLightPos"), (GLsizei)lamps.size(), glm::value_ptr(lamps[0]));
    glUniform3fv(glGetUniformLocation(program, "pointLightColor"), 1, glm::value_ptr(LAMP_COLOR));
    // the shadows: the cascades are already bound by the main pass (bindShadowMaps), same uniforms
    glUniform1i(glGetUniformLocation(program, "shadowsEnabled"), shadowsEnabled);
    glUniform1i(glGetUniformLocation(program, "shadowMap"), SHADOW_MAP_TEXTURE_UNIT);
    glUniformMatrix4fv(glGetUniformLocation(program, "lightSpaceMatrices"), SHADOW_CASCADES, GL_FALSE,
        glm::value_ptr(lightSpaceMatrices[0]));
    glUniform1fv(glGetUniformLocation(program, "cascadeSplits"), SHADOW_CASCADES + 1, CASCADE_SPLITS);
    glActiveTexture(GL_TEXTURE0 + GRASS_MAP_TEXTURE_UNIT);
    glBindTexture(GL_TEXTURE_2D, grassMapTexture);
    glUniform1i(glGetUniformLocation(program, "grassMap"), GRASS_MAP_TEXTURE_UNIT);
    glActiveTexture(GL_TEXTURE0);
}

void renderGrass() {
    if (!grassEnabled || snowCover >= 0.99f) return;
    glDisable(GL_CULL_FACE);   // a blade / a tuft is seen from both sides

    grassShader.useShaderProgram();
    setGrassUniforms(grassShader.shaderProgram, GRASS_GRID, GRASS_SPACING);
    glBindVertexArray(grassVAO);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 7, GRASS_GRID * GRASS_GRID);

    if (tuftTexture != 0) {
        tuftShader.useShaderProgram();
        GLuint program = tuftShader.shaderProgram;
        setGrassUniforms(program, TUFT_GRID, TUFT_SPACING);
        glUniform1f(glGetUniformLocation(program, "bladeRadius"), 0.5f * GRASS_GRID * GRASS_SPACING);
        glUniform1i(glGetUniformLocation(program, "flowersEnabled"), flowersEnabled);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tuftTexture);
        glUniform1i(glGetUniformLocation(program, "tuftTexture"), 0);
        // the edges of the pictures: alpha to coverage (smooth with MSAA, no sorting needed), the alpha written is 1
        glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
        glEnable(GL_SAMPLE_ALPHA_TO_ONE);
        glBindVertexArray(tuftVAO);
        glDrawArraysInstanced(GL_TRIANGLES, 0, 12, TUFT_GRID * TUFT_GRID);
        glDisable(GL_SAMPLE_ALPHA_TO_ONE);
        glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    }
    glBindVertexArray(0);
    glEnable(GL_CULL_FACE);
}

// ==============================================================================================================
// Smoke
// ==============================================================================================================

bool smokeEnabled = true;
gps::Shader smokeShader;
GLuint smokeVAO = 0, smokeQuadVBO = 0, smokeSeedVBO = 0;
int smokeMaxParticles = 0;

// positions in the power plant model (models/power_plant), moved to where the plant stands
const std::vector<SmokeEmitter> SMOKE_EMITTERS = {
    // the ventilation stack (60 m): a thin grey plume
    { POWER_PLANT_POS + glm::vec3(-89.0f, 60.5f, -4.0f), 1.5f, 300, 25.0f, 4.0f, 3.0f, 26.0f,
      glm::vec3(0.62f, 0.62f, 0.64f), 0.35f },
    // the cooling tower (72 m, 26 m wide at the top): broad white steam
    { POWER_PLANT_POS + glm::vec3(225.0f, 72.0f, -15.0f), 18.0f, 400, 26.0f, 2.5f, 14.0f, 42.0f,
      glm::vec3(0.95f, 0.95f, 0.97f), 0.2f },
};

void initSmoke() {
    smokeShader.loadShader("shaders/smoke.vert", "shaders/smoke.frag");

    // one quad (triangle strip), drawn once per particle (instancing); each instance has its own random seed
    const float corners[] = { -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f };
    for (const SmokeEmitter& e : SMOKE_EMITTERS) smokeMaxParticles = glm::max(smokeMaxParticles, e.particles);
    std::mt19937 rng(11);
    std::uniform_real_distribution<float> random01(0.0f, 1.0f);
    std::vector<glm::vec4> seeds;
    for (int i = 0; i < smokeMaxParticles; i++) {
        seeds.push_back(glm::vec4(random01(rng), random01(rng), random01(rng), random01(rng)));
    }

    glGenVertexArrays(1, &smokeVAO);
    glBindVertexArray(smokeVAO);
    glGenBuffers(1, &smokeQuadVBO);
    glBindBuffer(GL_ARRAY_BUFFER, smokeQuadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(corners), corners, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glGenBuffers(1, &smokeSeedVBO);
    glBindBuffer(GL_ARRAY_BUFFER, smokeSeedVBO);
    glBufferData(GL_ARRAY_BUFFER, seeds.size() * sizeof(glm::vec4), seeds.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribDivisor(1, 1);   // one seed per particle, not per vertex
    glBindVertexArray(0);
}

void renderSmoke() {
    if (!smokeEnabled) return;
    smokeShader.useShaderProgram();
    GLuint program = smokeShader.shaderProgram;
    glUniformMatrix4fv(glGetUniformLocation(program, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(program, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniform1f(glGetUniformLocation(program, "time"), (float)glfwGetTime());
    glm::vec2 wind = windSpeed * glm::vec2(cos(glm::radians(windDirection)), sin(glm::radians(windDirection)));
    glUniform2fv(glGetUniformLocation(program, "wind"), 1, glm::value_ptr(wind));
    glUniform3fv(glGetUniformLocation(program, "lightColor"), 1, glm::value_ptr(lightColor));
    glUniform1f(glGetUniformLocation(program, "dayFactor"), dayFactor);
    glUniform1f(glGetUniformLocation(program, "fogDensity"), fogEnabled ? fogDensity : 0.0f);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);   // hidden by the buildings in front, but they don't hide each other
    glDisable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glBindVertexArray(smokeVAO);
    for (const SmokeEmitter& e : SMOKE_EMITTERS) {
        glUniform3fv(glGetUniformLocation(program, "source"), 1, glm::value_ptr(e.position));
        glUniform1f(glGetUniformLocation(program, "sourceRadius"), e.radius);
        glUniform1f(glGetUniformLocation(program, "lifetime"), e.lifetime);
        glUniform1f(glGetUniformLocation(program, "riseSpeed"), e.riseSpeed);
        glUniform1f(glGetUniformLocation(program, "startSize"), e.startSize);
        glUniform1f(glGetUniformLocation(program, "endSize"), e.endSize);
        glUniform3fv(glGetUniformLocation(program, "smokeColor"), 1, glm::value_ptr(e.color));
        glUniform1f(glGetUniformLocation(program, "opacity"), e.opacity);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, glm::min(e.particles, smokeMaxParticles));
    }
    glBindVertexArray(0);
    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    applyViewMode();
}

// ==============================================================================================================
// Birds
// ==============================================================================================================

bool birdsEnabled = true;
std::vector<Bird> birds;
gps::Shader birdShader;
GLuint birdVAO = 0, birdMeshVBO = 0, birdInstanceVBO = 0;
int birdVertexCount = 0;
float birdTime = 0.0f;

// each flock roams around its own part of the town
struct FlockArea {
    glm::vec3 center;    // y = height above the ground
    float radius;
    float speed;         // how fast its target moves (radians per second along its loop)
};
const FlockArea FLOCK_AREAS[BIRD_FLOCKS] = {
    { glm::vec3(0.0f, 55.0f, 150.0f), 150.0f, 0.05f },     // over the Administrative Palace and its plaza
    { glm::vec3(-420.0f, 70.0f, 200.0f), 120.0f, 0.06f },  // around Genex
    { glm::vec3(250.0f, 45.0f, 0.0f), 160.0f, 0.04f },     // over the river
};

// the point a flock heads for: a slow loop (Lissajous curve) over its area, rising and falling a little
glm::vec3 flockTarget(int flock, float t) {
    const FlockArea& a = FLOCK_AREAS[flock];
    float w = a.speed * t + flock * 2.1f;
    glm::vec3 p = a.center + glm::vec3(a.radius * sin(w), 12.0f * sin(w * 2.3f), a.radius * 0.7f * cos(w * 1.3f));
    p.y += groundHeight(p.x, p.z);
    return p;
}

// bird mesh, model space: x = right wing, y = up, z = forward (meters, ~1.6 m wingspan, like a gull).
// The 4th value is how far along the wing the vertex is (0 = body, 1 = wing tip): it decides how much it flaps.
const float BIRD_MESH[] = {
    // body
    -0.06f, 0.0f,  0.00f, 0.0f,   0.06f, 0.0f,  0.00f, 0.0f,   0.00f, 0.02f,  0.35f, 0.0f,
    -0.06f, 0.0f,  0.00f, 0.0f,   0.00f, 0.0f, -0.30f, 0.0f,   0.06f, 0.0f,  0.00f, 0.0f,
    // tail
    -0.10f, 0.0f, -0.42f, 0.0f,   0.00f, 0.0f, -0.25f, 0.0f,   0.10f, 0.0f, -0.42f, 0.0f,
    // left wing: root -> middle -> tip
    -0.05f, 0.0f,  0.10f, 0.0f,  -0.40f, 0.0f,  0.04f, 0.5f,  -0.05f, 0.0f, -0.10f, 0.0f,
    -0.40f, 0.0f,  0.04f, 0.5f,  -0.80f, 0.0f, -0.15f, 1.0f,  -0.05f, 0.0f, -0.10f, 0.0f,
    // right wing
     0.05f, 0.0f,  0.10f, 0.0f,   0.40f, 0.0f,  0.04f, 0.5f,   0.05f, 0.0f, -0.10f, 0.0f,
     0.40f, 0.0f,  0.04f, 0.5f,   0.80f, 0.0f, -0.15f, 1.0f,   0.05f, 0.0f, -0.10f, 0.0f,
};

struct BirdInstance {
    glm::vec3 position;
    glm::vec3 forward;
    float phase;
    float flap;
};

void initBirds() {
    birdShader.loadShader("shaders/bird.vert", "shaders/bird.frag");

    std::mt19937 rng(5);
    std::uniform_real_distribution<float> random11(-1.0f, 1.0f);
    std::uniform_real_distribution<float> random01(0.0f, 1.0f);
    for (int f = 0; f < BIRD_FLOCKS; f++) {
        glm::vec3 start = flockTarget(f, 0.0f);
        for (int i = 0; i < BIRDS_PER_FLOCK; i++) {
            Bird b;
            b.position = start + glm::vec3(random11(rng) * 15.0f, random11(rng) * 5.0f, random11(rng) * 15.0f);
            b.velocity = glm::vec3(random11(rng), 0.1f * random11(rng), random11(rng)) * BIRD_MIN_SPEED;
            b.phase = random01(rng) * glm::two_pi<float>();
            b.flap = 1.0f;
            b.flock = f;
            birds.push_back(b);
        }
    }

    birdVertexCount = (int)(sizeof(BIRD_MESH) / sizeof(float) / 4);
    glGenVertexArrays(1, &birdVAO);
    glBindVertexArray(birdVAO);
    glGenBuffers(1, &birdMeshVBO);
    glBindBuffer(GL_ARRAY_BUFFER, birdMeshVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(BIRD_MESH), BIRD_MESH, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // per bird (instancing): position, forward direction, wing beat
    glGenBuffers(1, &birdInstanceVBO);
    glBindBuffer(GL_ARRAY_BUFFER, birdInstanceVBO);
    glBufferData(GL_ARRAY_BUFFER, birds.size() * sizeof(BirdInstance), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(BirdInstance), (void*)offsetof(BirdInstance, position));
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(BirdInstance), (void*)offsetof(BirdInstance, forward));
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(BirdInstance), (void*)offsetof(BirdInstance, phase));
    for (int i = 1; i <= 3; i++) {
        glEnableVertexAttribArray(i);
        glVertexAttribDivisor(i, 1);
    }
    glBindVertexArray(0);
}

glm::vec3 limitLength(const glm::vec3& v, float maxLength) {
    float l = glm::length(v);
    return l > maxLength ? v * (maxLength / l) : v;
}

void updateBirds() {
    float dt = glm::min(deltaTime, 0.05f);   // a long frame must not throw the birds far away
    birdTime += dt;
    std::vector<glm::vec3> acceleration(birds.size(), glm::vec3(0.0f));
    for (size_t i = 0; i < birds.size(); i++) {
        Bird& b = birds[i];
        // the three rules, over the neighbours of the same flock
        glm::vec3 separation(0.0f), averageVelocity(0.0f), averagePosition(0.0f);
        int neighbours = 0;
        for (size_t j = 0; j < birds.size(); j++) {
            if (j == i || birds[j].flock != b.flock) continue;
            glm::vec3 d = b.position - birds[j].position;
            float distance = glm::length(d);
            if (distance > BIRD_NEIGHBOUR_RADIUS) continue;
            if (distance < BIRD_SEPARATION_RADIUS && distance > 1e-3f) {
                separation += d / (distance * distance);   // the closer, the stronger the push away
            }
            averageVelocity += birds[j].velocity;
            averagePosition += birds[j].position;
            neighbours++;
        }
        glm::vec3 a = 8.0f * separation;
        if (neighbours > 0) {
            a += 1.0f * (averageVelocity / (float)neighbours - b.velocity);   // alignment
            a += 0.4f * (averagePosition / (float)neighbours - b.position);  // cohesion
        }
        // the flock's target: steer towards it at full speed
        glm::vec3 toTarget = flockTarget(b.flock, birdTime) - b.position;
        if (glm::length(toTarget) > 1e-3f) {
            a += 0.6f * (glm::normalize(toTarget) * BIRD_MAX_SPEED - b.velocity);
        }
        // keep above the ground, and around the buildings: look a little ahead; if that point is inside a building,
        // climb and turn to the side
        float ground = groundHeight(b.position.x, b.position.z);
        if (b.position.y < ground + 20.0f) a.y += 10.0f;
        glm::vec3 ahead = b.position + glm::normalize(b.velocity) * 10.0f;
        if (collides(ahead)) {
            a += glm::vec3(0.0f, 15.0f, 0.0f) + 10.0f * glm::normalize(glm::cross(b.velocity, glm::vec3(0.0f, 1.0f, 0.0f)));
        }
        acceleration[i] = limitLength(a, 25.0f);
    }
    for (size_t i = 0; i < birds.size(); i++) {
        Bird& b = birds[i];
        b.velocity += acceleration[i] * dt;
        float speed = glm::clamp(glm::length(b.velocity), BIRD_MIN_SPEED, BIRD_MAX_SPEED);
        b.velocity = glm::normalize(b.velocity) * speed;
        b.position += b.velocity * dt;
        // flapping while climbing or speeding up, gliding while descending
        float wantFlap = b.velocity.y > -0.5f ? 1.0f : 0.15f;
        b.flap += (wantFlap - b.flap) * glm::min(dt * 2.0f, 1.0f);
        b.phase = fmod(b.phase + dt * glm::two_pi<float>() * 3.5f * (0.3f + 0.7f * b.flap), glm::two_pi<float>());
    }
}

void renderBirds() {
    if (!birdsEnabled || dayFactor < 0.25f) return;   // at night they roost
    std::vector<BirdInstance> instances;
    instances.reserve(birds.size());
    for (const Bird& b : birds) {
        instances.push_back({ b.position, glm::normalize(b.velocity), b.phase, b.flap });
    }
    glBindBuffer(GL_ARRAY_BUFFER, birdInstanceVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, instances.size() * sizeof(BirdInstance), instances.data());

    birdShader.useShaderProgram();
    GLuint program = birdShader.shaderProgram;
    glUniformMatrix4fv(glGetUniformLocation(program, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(program, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniform3fv(glGetUniformLocation(program, "lightColor"), 1, glm::value_ptr(lightColor));
    glUniform1f(glGetUniformLocation(program, "dayFactor"), dayFactor);
    glUniform1f(glGetUniformLocation(program, "fogDensity"), fogEnabled ? fogDensity : 0.0f);

    glEnable(GL_BLEND);   // they fade in the fog
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);   // thin wings, seen from above and from below
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glBindVertexArray(birdVAO);
    glDrawArraysInstanced(GL_TRIANGLES, 0, birdVertexCount, (GLsizei)instances.size());
    glBindVertexArray(0);
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    applyViewMode();
}
