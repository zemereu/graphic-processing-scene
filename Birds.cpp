#include "Birds.hpp"
#include "Collision.hpp"   // groundHeight, collides
#include "Lighting.hpp"    // light, dayFactor, fog
#include "Renderer.hpp"    // applyViewMode

#include <random>

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
