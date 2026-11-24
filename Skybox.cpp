#include "Skybox.hpp"
#include "Lighting.hpp"   // dayFactor, sky angles, fog
#include "Renderer.hpp"   // applyViewMode
#include "Rain.hpp"       // lightning

#include "stb_image.h"

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
