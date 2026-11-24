#include "Lighting.hpp"
#include "Renderer.hpp"   // flatShading

// light parameters
glm::vec3 lightDir;
glm::vec3 lightColor;
GLint skyLightLoc;

float timeOfDay = DAY_TIME;
bool dayCycleRunning = false;
float dayCycleSpeed = 0.1f;   // a whole day in 4 minutes
float dayFactor = 1.0f;
float daySkyAngle = 0.0f, nightSkyAngle = 0.0f;
bool windowsLit = false, nightMode = false, floodlightsOn = false;
const std::vector<glm::vec3> LAMP_POSITIONS = { // base of each lamp post (max 16, see basic.frag)
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
const std::vector<SpotLight> SPOT_LIGHTS = { // max 4, see basic.frag
    {{-5.0f, 15.3f, 86.0f}, {0.0f, 60.0f, 70.0f}},  // on the roof of the podium (y = 13), aimed at the main tower
    {{5.0f, 15.3f, 86.0f},  {0.0f, 60.0f, 70.0f}},  // (position = lamp heads; the tripod stands 2.3 m below)
    {{25.0f, 15.3f, 86.0f}, {13.0f, 40.0f, 68.0f}}, // on the podium, aimed at the east tower
};

bool fogEnabled = true;
float fogDensity = 0.0012f;   // the world is 2 km wide: lighter fog than before

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

    // the directional light (sun / moon): direction and color are set every frame by updateDayCycle()
    lightDirLoc = glGetUniformLocation(myBasicShader.shaderProgram, "lightDir");
    lightColorLoc = glGetUniformLocation(myBasicShader.shaderProgram, "lightColor");
    skyLightLoc = glGetUniformLocation(myBasicShader.shaderProgram, "skyLight");

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
    // dusk: the sun is (almost) gone but the sky is still bright; its light makes up for what the sun no longer gives
    glm::vec3 skyLight = SKY_LIGHT_COLOR * glm::max(dayFactor - sunStrength, 0.0f);

    // the photos have the sun / moon glow towards +Z: turn each sky so its glow follows the real light
    daySkyAngle = atan2(sun.x, sun.z);
    nightSkyAngle = atan2(moon.x, moon.z);

    myBasicShader.useShaderProgram();
    glUniform3fv(lightDirLoc, 1, glm::value_ptr(lightDir));
    glUniform3fv(lightColorLoc, 1, glm::value_ptr(lightColor));
    glUniform3fv(skyLightLoc, 1, glm::value_ptr(skyLight));

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
