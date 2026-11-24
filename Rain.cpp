#include "Rain.hpp"
#include "Lighting.hpp"   // nightMode

#include <random>

const int RAIN_DROPS = 20000;
const float RAIN_FALL_SPEED = 25.0f;   // meters per second
const float RAIN_AREA_SIZE = 80.0f;    // the rain box around the camera, meters
const float RAIN_AREA_HEIGHT = 40.0f;
const float RAIN_DROP_LENGTH = 0.8f;
bool rainEnabled = false;
float wetness = 0.0f;
GLint wetnessLoc;   // in myBasicShader
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
    wetnessLoc = glGetUniformLocation(myBasicShader.shaderProgram, "wetness");
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
    myBasicShader.useShaderProgram();
    glUniform1f(wetnessLoc, wetness);

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
