#include "Lighting.hpp"
#include "Renderer.hpp"   // flatShading

// light parameters
glm::vec3 lightDir;
glm::vec3 lightColor;

bool nightMode = false;
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
    fogColorLoc = glGetUniformLocation(myBasicShader.shaderProgram, "fogColor");
    applyFog();

    applyLighting();
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
