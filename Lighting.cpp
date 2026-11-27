#include "Lighting.hpp"
#include "Renderer.hpp"
#include "Environment.hpp"
#include "stb_image.h"

#include <cfloat>

// ==============================================================================================================
// Lighting
// ==============================================================================================================

// light parameters
glm::vec3 lightDir;
glm::vec3 lightColor;

float timeOfDay = DAY_TIME;
bool dayCycleRunning = false;
float dayCycleSpeed = 0.1f;   // a whole day in 4 minutes
float dayFactor = 1.0f;
float daySkyAngle = 0.0f, nightSkyAngle = 0.0f;
bool windowsLit = false, nightMode = false, floodlightsOn = false;
std::vector<glm::vec3> LAMP_POSITIONS = { // base of each lamp post (max 16, see basic.frag)
    // plaza in front of the Administrative Palace
    {-60.0f, 0.0f, 130.0f}, {-20.0f, 0.0f, 130.0f}, {20.0f, 0.0f, 130.0f}, {60.0f, 0.0f, 130.0f},
    {-60.0f, 0.0f, 170.0f}, {-20.0f, 0.0f, 170.0f}, {20.0f, 0.0f, 170.0f}, {60.0f, 0.0f, 170.0f},
    // plaza of Palatul Telefoanelor
    {-195.0f, 0.0f, 175.0f}, {-145.0f, 0.0f, 175.0f},
    // road in front of Genex
    {-455.0f, 0.0f, 186.0f}, {-385.0f, 0.0f, 186.0f},
    // crossing of the boulevard with the highway, both ends of the bridge (on the sidewalk / quay)
    {89.1f, 0.4f, 186.0f}, {110.9f, 0.4f, 206.0f}, {84.0f, 0.3f, 35.06f}, {116.0f, 0.3f, 39.08f},
};
std::vector<SpotLight> SPOT_LIGHTS = { // max 4, see basic.frag
    {{-5.0f, 15.3f, 86.0f}, {0.0f, 60.0f, 70.0f}},  // on the roof of the podium (y = 13), aimed at the main tower
    {{5.0f, 15.3f, 86.0f},  {0.0f, 60.0f, 70.0f}},  // (position = lamp heads; the tripod stands 2.3 m below)
    {{25.0f, 15.3f, 86.0f}, {13.0f, 40.0f, 68.0f}}, // on the podium, aimed at the east tower
};

bool fogEnabled = true;
float fogDensity = 0.0012f;   // the world is 2 km wide: lighter fog than before

void initUniforms() {
    myBasicShader.useShaderProgram();
    // nothing dithered out until an object asks for it (Renderer: renderObject)
    glUniform1f(glGetUniformLocation(myBasicShader.shaderProgram, "ditherKeep"), 1.0f);

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

    // the directional light (sun / moon): direction and color are set every frame by updateDayCycle()
    lightDirLoc = glGetUniformLocation(myBasicShader.shaderProgram, "lightDir");
    lightColorLoc = glGetUniformLocation(myBasicShader.shaderProgram, "lightColor");

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
    paintColorLoc = glGetUniformLocation(myBasicShader.shaderProgram, "paintColor");
    surfaceModeLoc = glGetUniformLocation(myBasicShader.shaderProgram, "surfaceMode");
    timeLoc = glGetUniformLocation(myBasicShader.shaderProgram, "time");

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
    applyFog();

    updateDayCycle();
    applyLighting();
}

// direction towards the sun at this hour (equinox: 12 hours of day, 12 of night)
glm::vec3 sunDirection(float hours) {
    float hourAngle = glm::radians((hours - 12.0f) * 15.0f);   // the sky turns 15 degrees per hour
    float latitude = glm::radians(LATITUDE);
    // x = south, y = up, z = west
    return glm::vec3(sin(latitude) * cos(hourAngle), cos(latitude) * cos(hourAngle), sin(hourAngle));
}

void updateDayCycle() {
    if (dayCycleRunning) {
        timeOfDay = fmod(timeOfDay + dayCycleSpeed * deltaTime, 24.0f);
    }
    glm::vec3 sun = sunDirection(timeOfDay);
    glm::vec3 moon = -sun;

    // the sky: the sunset photo while the sun is up, fading into the night photo until it is ~9 degrees below the
    // horizon (twilight)
    dayFactor = glm::smoothstep(-0.15f, 0.05f, sun.y);

    // the light comes from whichever of the two is above the horizon; at the switch both are dim, so it doesn't jump
    lightDir = sun.y > 0.0f ? sun : moon;
    // the sun weakens and turns orange towards the horizon; below it, only the moon is left
    float sunStrength = glm::smoothstep(-0.05f, 0.25f, sun.y);
    glm::vec3 sunColor = glm::mix(SUNSET_COLOR, SUN_COLOR, glm::smoothstep(0.0f, 0.45f, sun.y));
    lightColor = glm::mix(MOON_COLOR, sunColor, sunStrength);

    // the photos have the sun / moon glow towards +Z: turn each sky so its glow follows the real light
    daySkyAngle = atan2(sun.x, sun.z);
    nightSkyAngle = atan2(moon.x, moon.z);

    myBasicShader.useShaderProgram();
    glUniform3fv(lightDirLoc, 1, glm::value_ptr(lightDir));
    glUniform3fv(lightColorLoc, 1, glm::value_ptr(lightColor));

    // the town's lights, one group after the other
    bool windows = dayFactor < 0.85f, lamps = dayFactor < 0.6f, floodlights = dayFactor < 0.4f;
    if (windows != windowsLit || lamps != nightMode || floodlights != floodlightsOn) {
        windowsLit = windows;
        nightMode = lamps;
        floodlightsOn = floodlights;
        applyLighting();
    }
}

void toggleDayNight() {
    timeOfDay = dayFactor > 0.5f ? NIGHT_TIME : DAY_TIME;
    updateDayCycle();
}

void applyLighting() {
    myBasicShader.useShaderProgram();
    glUniform1i(numPointLightsLoc, nightMode ? (GLint)LAMP_POSITIONS.size() : 0);
    glUniform1i(numSpotLightsLoc, floodlightsOn ? (GLint)SPOT_LIGHTS.size() : 0);
    // background behind the skybox (only visible if the skybox fails to load); the fog color comes from the skybox
    glm::vec3 sky = nightMode ? NIGHT_SKY : DAY_SKY;
    glClearColor(sky.r, sky.g, sky.b, 1.0f);
}

void applyFog() {
    myBasicShader.useShaderProgram();
    glUniform1f(fogDensityLoc, fogEnabled ? fogDensity : 0.0f);
}

// ==============================================================================================================
// Skybox
// ==============================================================================================================

gps::Shader skyboxShader;
GLuint daySkyTexture = 0, nightSkyTexture = 0;
GLuint skyboxVAO = 0, skyboxVBO = 0;
GLint skyViewLoc, skyProjectionLoc, skyFogDensityLoc;   // in skyboxShader

// the 6 faces in the order OpenGL numbers them: +X, -X, +Y, -Y, +Z, -Z
// (made from the panoramas with tools/equirect_to_cubemap.py, rotated so the sun / moon glow is towards +Z)
GLuint loadCubemap(const std::string& folder) {
    const char* faces[6] = { "right", "left", "top", "bottom", "back", "front" };
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_CUBE_MAP, texture);
    for (int i = 0; i < 6; i++) {
        std::string path = folder + "/" + faces[i] + ".jpg";
        int width, height, channels;
        unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 4);
        if (!data) {
            std::cerr << "Skybox face failed to load: " << path << std::endl;
            continue;
        }
        // sRGB photo: the GPU converts it to linear colors when sampling, like the models' color textures
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_SRGB8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        stbi_image_free(data);
    }
    // mipmaps: the fog reads a blurred level of the sky
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    return texture;
}

void initSkybox() {
    skyboxShader.loadShader("shaders/skybox.vert", "shaders/skybox.frag");
    // filter across the edges of the faces too (no visible seams, especially in the blurred mipmaps)
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    daySkyTexture = loadCubemap("models/skybox/day");
    nightSkyTexture = loadCubemap("models/skybox/night");

    // unit cube around the camera; seen from the inside
    const float v[] = {
        -1, 1,-1,  -1,-1,-1,   1,-1,-1,   1,-1,-1,   1, 1,-1,  -1, 1,-1,
        -1,-1, 1,  -1,-1,-1,  -1, 1,-1,  -1, 1,-1,  -1, 1, 1,  -1,-1, 1,
         1,-1,-1,   1,-1, 1,   1, 1, 1,   1, 1, 1,   1, 1,-1,   1,-1,-1,
        -1,-1, 1,  -1, 1, 1,   1, 1, 1,   1, 1, 1,   1,-1, 1,  -1,-1, 1,
        -1, 1,-1,   1, 1,-1,   1, 1, 1,   1, 1, 1,  -1, 1, 1,  -1, 1,-1,
        -1,-1,-1,  -1,-1, 1,   1,-1,-1,   1,-1,-1,  -1,-1, 1,   1,-1, 1,
    };
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);

    skyViewLoc = glGetUniformLocation(skyboxShader.shaderProgram, "view");
    skyProjectionLoc = glGetUniformLocation(skyboxShader.shaderProgram, "projection");
    skyFogDensityLoc = glGetUniformLocation(skyboxShader.shaderProgram, "fogDensity");
    // both shaders read the skies the same way: same texture units, same night brightness
    for (gps::Shader* shader : { &skyboxShader, &myBasicShader }) {
        shader->useShaderProgram();
        glUniform1i(glGetUniformLocation(shader->shaderProgram, "daySky"), DAY_SKY_TEXTURE_UNIT);
        glUniform1i(glGetUniformLocation(shader->shaderProgram, "nightSky"), NIGHT_SKY_TEXTURE_UNIT);
        glUniform1f(glGetUniformLocation(shader->shaderProgram, "nightSkyBrightness"), NIGHT_SKY_BRIGHTNESS);
    }
}

// how much of each sky (dayFactor), how they are turned, and their textures; the shader must be in use
void setSkyUniforms(gps::Shader& shader) {
    glUniform1f(glGetUniformLocation(shader.shaderProgram, "daylight"), dayFactor);
    glUniform1f(glGetUniformLocation(shader.shaderProgram, "daySkyAngle"), daySkyAngle);
    glUniform1f(glGetUniformLocation(shader.shaderProgram, "nightSkyAngle"), nightSkyAngle);
    glUniform3fv(glGetUniformLocation(shader.shaderProgram, "sunDirection"), 1, glm::value_ptr(sunDirection(timeOfDay)));
    glUniform3fv(glGetUniformLocation(shader.shaderProgram, "flashDirection"), 1, glm::value_ptr(flashDirection));
    glUniform3fv(glGetUniformLocation(shader.shaderProgram, "flashColor"), 1, glm::value_ptr(flashColor));
    glActiveTexture(GL_TEXTURE0 + DAY_SKY_TEXTURE_UNIT);
    glBindTexture(GL_TEXTURE_CUBE_MAP, daySkyTexture);
    glActiveTexture(GL_TEXTURE0 + NIGHT_SKY_TEXTURE_UNIT);
    glBindTexture(GL_TEXTURE_CUBE_MAP, nightSkyTexture);
    glActiveTexture(GL_TEXTURE0);
}

void bindSkyTexture() {
    myBasicShader.useShaderProgram();
    setSkyUniforms(myBasicShader);
}

void renderSkybox() {
    skyboxShader.useShaderProgram();
    // only the camera's rotation: the sky is infinitely far, it never gets closer
    glm::mat4 skyView = glm::mat4(glm::mat3(view));
    glUniformMatrix4fv(skyViewLoc, 1, GL_FALSE, glm::value_ptr(skyView));
    glUniformMatrix4fv(skyProjectionLoc, 1, GL_FALSE, glm::value_ptr(projection));
    glUniform1f(skyFogDensityLoc, fogEnabled ? fogDensity : 0.0f);
    setSkyUniforms(skyboxShader);

    // the sky is written at depth 1 (the far plane): it passes only where the depth buffer is still clear
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);                     // seen from the inside
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);   // the wireframe/points view modes apply to the objects
    glBindVertexArray(skyboxVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    applyViewMode();
}

// ==============================================================================================================
// Shadows
// ==============================================================================================================

bool shadowsEnabled = true;
bool showShadowMap = false;
GLuint depthMapFBO = 0, depthMapTexture = 0;
gps::Shader depthMapShader;
GLint depthModelLoc, depthLightSpaceLoc;           // in depthMapShader
glm::mat4 lightSpaceMatrices[SHADOW_CASCADES];
float cascadeTexel[SHADOW_CASCADES];               // meters covered by one texel of each cascade
float cascadeDepthRange[SHADOW_CASCADES];          // meters between the near and far planes of each cascade
GLint cascadeSplitsLoc, cascadeTexelLoc, cascadeDepthRangeLoc;   // in myBasicShader
gps::Shader shadowPreviewShader;
GLuint shadowPreviewFBO[SHADOW_CASCADES] = { 0 }, shadowPreviewTexture[SHADOW_CASCADES] = { 0 };
const int SHADOW_PREVIEW_SIZE = 256;
bool depthPass = false;                  // true while drawing into a shadow map
glm::mat4 depthPassMatrix;
bool spotShadowPass = false;
GLuint spotShadowFBO = 0, spotShadowTexture = 0;
glm::mat4 spotLightSpaceMatrices[4];
GLint spotLightSpaceLoc, spotShadowMapsLoc, spotShadowNearFarLoc, numSpotShadowsLoc;   // in myBasicShader

void initShadowMap() {
    // depth textures: what the light "sees" (distance to the closest surface for each texel), one layer per cascade
    glGenTextures(1, &depthMapTexture);
    glBindTexture(GL_TEXTURE_2D_ARRAY, depthMapTexture);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT24, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE, SHADOW_CASCADES, 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    // outside the shadow map: depth 1 = nothing in front = lit
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float border[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, border);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    // framebuffer with only a depth attachment (one layer of the array at a time), no color
    glGenFramebuffers(1, &depthMapFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthMapTexture, 0, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "The shadow map framebuffer is incomplete" << std::endl;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // floodlights: a depth texture array, one layer per spot light; the framebuffer gets one layer at a time
    glGenTextures(1, &spotShadowTexture);
    glBindTexture(GL_TEXTURE_2D_ARRAY, spotShadowTexture);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT24, SPOT_SHADOW_MAP_SIZE, SPOT_SHADOW_MAP_SIZE, 4, 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, border);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    glGenFramebuffers(1, &spotShadowFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, spotShadowFBO);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, spotShadowTexture, 0, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "The floodlight shadow framebuffer is incomplete" << std::endl;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    spotLightSpaceLoc = glGetUniformLocation(myBasicShader.shaderProgram, "spotLightSpace");
    spotShadowMapsLoc = glGetUniformLocation(myBasicShader.shaderProgram, "spotShadowMaps");
    spotShadowNearFarLoc = glGetUniformLocation(myBasicShader.shaderProgram, "spotShadowNearFar");
    numSpotShadowsLoc = glGetUniformLocation(myBasicShader.shaderProgram, "numSpotShadows");

    // preview of the cascades for the UI: small grayscale images
    shadowPreviewShader.loadShader("shaders/screen.vert", "shaders/shadow_preview.frag");
    for (int i = 0; i < SHADOW_CASCADES; i++) {
        glGenTextures(1, &shadowPreviewTexture[i]);
        glBindTexture(GL_TEXTURE_2D, shadowPreviewTexture[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, SHADOW_PREVIEW_SIZE, SHADOW_PREVIEW_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glGenFramebuffers(1, &shadowPreviewFBO[i]);
        glBindFramebuffer(GL_FRAMEBUFFER, shadowPreviewFBO[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, shadowPreviewTexture[i], 0);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    depthModelLoc = glGetUniformLocation(depthMapShader.shaderProgram, "model");
    depthLightSpaceLoc = glGetUniformLocation(depthMapShader.shaderProgram, "lightSpaceTrMatrix");
    lightSpaceTrMatrixLoc = glGetUniformLocation(myBasicShader.shaderProgram, "lightSpaceMatrices");
    shadowMapLoc = glGetUniformLocation(myBasicShader.shaderProgram, "shadowMap");
    shadowsEnabledLoc = glGetUniformLocation(myBasicShader.shaderProgram, "shadowsEnabled");
    cascadeSplitsLoc = glGetUniformLocation(myBasicShader.shaderProgram, "cascadeSplits");
    cascadeTexelLoc = glGetUniformLocation(myBasicShader.shaderProgram, "cascadeTexel");
    cascadeDepthRangeLoc = glGetUniformLocation(myBasicShader.shaderProgram, "cascadeDepthRange");
}

// each cascade: the slice of the camera's view between two split distances is wrapped in a sphere, and the light's
// orthographic projection covers that sphere. A sphere looks the same from every direction, so the size of the
// projection doesn't change when the camera turns; with its center snapped to whole texels the shadow edges stay
// still (no shimmering) while the camera moves.
void computeLightSpaceMatrices(bool farToo) {
    glm::vec3 toLight = glm::normalize(lightDir);
    glm::vec3 up = fabs(toLight.y) > 0.99f ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
    glm::mat4 lightView = glm::lookAt(toLight, glm::vec3(0.0f), up);
    float aspect = (float)sceneWidth / (float)glm::max(sceneHeight, 1);

    for (int i = 0; i < SHADOW_CASCADES; i++) {
        if (i == SHADOW_CASCADES - 1 && !farToo) continue;   // kept as it was: its shadow map wasn't drawn again
        // the 8 corners of the slice (world space): the camera's frustum between the two split distances
        glm::mat4 slice = glm::perspective(glm::radians(45.0f), aspect, CASCADE_SPLITS[i], CASCADE_SPLITS[i + 1]);
        glm::mat4 toWorld = glm::inverse(slice * view);
        glm::vec3 corners[8];
        glm::vec3 center(0.0f);
        for (int c = 0; c < 8; c++) {
            glm::vec4 p = toWorld * glm::vec4((c & 1) ? 1.0f : -1.0f, (c & 2) ? 1.0f : -1.0f, (c & 4) ? 1.0f : -1.0f, 1.0f);
            corners[c] = glm::vec3(p) / p.w;
            center += corners[c] / 8.0f;
        }
        float radius = 0.0f;
        for (int c = 0; c < 8; c++) radius = glm::max(radius, glm::distance(center, corners[c]));
        radius = ceil(radius);

        glm::vec3 c = glm::vec3(lightView * glm::vec4(center, 1.0f));
        float texel = 2.0f * radius / SHADOW_MAP_SIZE;
        c.x = floor(c.x / texel) * texel;
        c.y = floor(c.y / texel) * texel;
        // the light looks down -z: near = the side towards the light (extended for the casters outside the sphere)
        glm::mat4 projection = glm::ortho(c.x - radius, c.x + radius, c.y - radius, c.y + radius,
                                          -(c.z + radius) - SHADOW_CASTER_RANGE, -(c.z - radius));
        lightSpaceMatrices[i] = projection * lightView;
        cascadeTexel[i] = texel;
        cascadeDepthRange[i] = 2.0f * radius + SHADOW_CASTER_RANGE;
    }
}

// pass 1: the scene from the sun / moon, depth only, once per cascade
void renderShadowMap() {
    if (!shadowsEnabled) return;
    // the farthest cascade: drawn again only every other frame (its texels are big, a frame late can't be seen);
    // its matrix is then kept too, so that its shadow map and its matrix always go together
    static unsigned int frame = 0;
    bool farToo = frame++ % 2 == 0;
    computeLightSpaceMatrices(farToo);
    depthMapShader.useShaderProgram();
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glViewport(0, 0, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);  // wireframe/points don't apply to the shadows
    depthPass = true;
    for (int i = 0; i < SHADOW_CASCADES; i++) {
        if (i == SHADOW_CASCADES - 1 && !farToo) continue;
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthMapTexture, 0, i);
        glClear(GL_DEPTH_BUFFER_BIT);
        glUniformMatrix4fv(depthLightSpaceLoc, 1, GL_FALSE, glm::value_ptr(lightSpaceMatrices[i]));
        depthPassMatrix = lightSpaceMatrices[i];   // culling: only what is in this cascade
        renderSceneObjects();
    }
    depthPass = false;
    applyViewMode();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, sceneWidth, sceneHeight);
}

// pass 1b: each floodlight sees the scene in its cone (perspective: a point light, the rays spread), depth only
void renderSpotShadowMaps() {
    if (!shadowsEnabled || !floodlightsOn) return;
    depthMapShader.useShaderProgram();
    glBindFramebuffer(GL_FRAMEBUFFER, spotShadowFBO);
    glViewport(0, 0, SPOT_SHADOW_MAP_SIZE, SPOT_SHADOW_MAP_SIZE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    depthPass = true;
    spotShadowPass = true;
    for (size_t i = 0; i < SPOT_LIGHTS.size() && i < 4; i++) {
        const SpotLight& spot = SPOT_LIGHTS[i];
        glm::vec3 dir = glm::normalize(spot.target - spot.position);
        glm::vec3 up = fabs(dir.y) > 0.9f ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
        // the frustum just covers the outer cone (+ a little margin)
        glm::mat4 projection = glm::perspective(glm::radians(2.0f * SPOT_OUTER_ANGLE + 4.0f), 1.0f,
                                                SPOT_SHADOW_NEAR, SPOT_SHADOW_FAR);
        spotLightSpaceMatrices[i] = projection * glm::lookAt(spot.position, spot.target, up);

        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, spotShadowTexture, 0, (GLint)i);
        glClear(GL_DEPTH_BUFFER_BIT);
        glUniformMatrix4fv(depthLightSpaceLoc, 1, GL_FALSE, glm::value_ptr(spotLightSpaceMatrices[i]));
        depthPassMatrix = spotLightSpaceMatrices[i];
        renderSceneObjects();
    }
    spotShadowPass = false;
    depthPass = false;
    applyViewMode();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, sceneWidth, sceneHeight);
}

// the shadow maps (sun / moon cascades and floodlights) for the main shader
void bindShadowMaps() {
    myBasicShader.useShaderProgram();
    glUniformMatrix4fv(lightSpaceTrMatrixLoc, SHADOW_CASCADES, GL_FALSE, glm::value_ptr(lightSpaceMatrices[0]));
    glUniform1fv(cascadeSplitsLoc, SHADOW_CASCADES + 1, CASCADE_SPLITS);
    glUniform1fv(cascadeTexelLoc, SHADOW_CASCADES, cascadeTexel);
    glUniform1fv(cascadeDepthRangeLoc, SHADOW_CASCADES, cascadeDepthRange);
    glUniform1i(shadowsEnabledLoc, shadowsEnabled);
    glActiveTexture(GL_TEXTURE0 + SHADOW_MAP_TEXTURE_UNIT);
    glBindTexture(GL_TEXTURE_2D_ARRAY, depthMapTexture);
    glUniform1i(shadowMapLoc, SHADOW_MAP_TEXTURE_UNIT);

    int spotShadows = (shadowsEnabled && floodlightsOn) ? (int)glm::min(SPOT_LIGHTS.size(), (size_t)4) : 0;
    glUniform1i(numSpotShadowsLoc, spotShadows);
    glUniformMatrix4fv(spotLightSpaceLoc, 4, GL_FALSE, glm::value_ptr(spotLightSpaceMatrices[0]));
    glUniform2f(spotShadowNearFarLoc, SPOT_SHADOW_NEAR, SPOT_SHADOW_FAR);
    glActiveTexture(GL_TEXTURE0 + SPOT_SHADOW_TEXTURE_UNIT);
    glBindTexture(GL_TEXTURE_2D_ARRAY, spotShadowTexture);
    glUniform1i(spotShadowMapsLoc, SPOT_SHADOW_TEXTURE_UNIT);
    glActiveTexture(GL_TEXTURE0);
}

// the cascades as small grayscale images for the UI (dark = close to the light)
void renderShadowPreview() {
    if (!showShadowMap || !shadowsEnabled) return;
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDisable(GL_DEPTH_TEST);
    shadowPreviewShader.useShaderProgram();
    glUniform1i(glGetUniformLocation(shadowPreviewShader.shaderProgram, "shadowMaps"), 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, depthMapTexture);
    glViewport(0, 0, SHADOW_PREVIEW_SIZE, SHADOW_PREVIEW_SIZE);
    glBindVertexArray(screenVAO);
    for (int i = 0; i < SHADOW_CASCADES; i++) {
        glBindFramebuffer(GL_FRAMEBUFFER, shadowPreviewFBO[i]);
        glUniform1i(glGetUniformLocation(shadowPreviewShader.shaderProgram, "layer"), i);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, sceneWidth, sceneHeight);
    glEnable(GL_DEPTH_TEST);
    applyViewMode();
}
