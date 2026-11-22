#include "Collision.hpp"
#include "Scene.hpp"
#include "Lighting.hpp"   // lamp posts and floodlights

#include <cfloat>
#include <fstream>

// collision detection: the camera is a sphere that can't enter the buildings' boxes
const float CAMERA_RADIUS = 1.5f;      // meters
const float CAMERA_MIN_HEIGHT = 1.8f;  // eye height above the ground (terrain height map)
bool collisionsEnabled = true;
std::vector<gps::BoundingBox> satuMareBounds, genexBounds, restaurantBounds, panoramaBounds,
                              telefoaneBounds, lampPostBounds, floodlightBounds; // model space, from the meshes
std::vector<gps::BoundingBox> worldColliders; // world space, rebuilt every frame (some objects move)
std::vector<gps::BoundingBox> motherlandBounds, romanitaBounds, powerPlantBounds, dugaBounds, pripyatBounds,
                              ferrisSupportBounds, ferrisWheelBounds;
std::vector<gps::BoundingBox> cityNorthBounds, citySouthBounds;

// terrain height map (written by tools/gen_world.py together with the terrain),
// used to keep the camera above the ground: heights on a regular grid, read once at startup
struct HeightMap {
    float x0 = 0.0f, z0 = 0.0f, step = 1.0f;   // world position of the first sample, grid spacing
    int nx = 0, nz = 0;
    std::vector<float> heights;                // nz rows of nx values
};
HeightMap terrainHeights;

bool loadHeightMap(const char* path) {
    std::ifstream in(path);
    if (!in) {
        std::cerr << "Could not open the height map " << path << std::endl;
        return false;
    }
    HeightMap& m = terrainHeights;
    in >> m.x0 >> m.z0 >> m.step >> m.nx >> m.nz;
    m.heights.resize((size_t)m.nx * m.nz);
    for (float& h : m.heights) in >> h;
    return (bool)in;
}

void initColliders() {
    cityNorthBounds = cityNorth.GetMeshBounds();
    citySouthBounds = citySouth.GetMeshBounds();
    loadHeightMap("models/terrain/terrain_height.txt");

    // collision shapes, computed once from the meshes
    satuMareBounds = satuMare.GetMeshBounds();
    genexBounds = genex.GetMeshBounds();
    restaurantBounds = genexRestaurant.GetMeshBounds();
    panoramaBounds = panorama.GetMeshBounds();
    telefoaneBounds = telefoane.GetMeshBounds();
    lampPostBounds = lampPost.GetMeshBounds();
    floodlightBounds = floodlight.GetMeshBounds();
    motherlandBounds = motherland.GetMeshBounds();
    romanitaBounds = romanita.GetMeshBounds();
    powerPlantBounds = powerPlant.GetMeshBounds();
    dugaBounds = duga.GetMeshBounds();
    pripyatBounds = pripyatBlocks.GetMeshBounds();
    ferrisSupportBounds = ferrisSupport.GetMeshBounds();
    ferrisWheelBounds = ferrisWheel.GetMeshBounds();
}

// box in model space -> box in world space that contains it (the 8 corners are transformed)
gps::BoundingBox transformBox(const gps::BoundingBox& box, const glm::mat4& m) {
    gps::BoundingBox result = { glm::vec3(FLT_MAX), glm::vec3(-FLT_MAX) };
    for (int i = 0; i < 8; i++) {
        glm::vec3 corner((i & 1) ? box.max.x : box.min.x,
                         (i & 2) ? box.max.y : box.min.y,
                         (i & 4) ? box.max.z : box.min.z);
        glm::vec3 p = glm::vec3(m * glm::vec4(corner, 1.0f));
        result.min = glm::min(result.min, p);
        result.max = glm::max(result.max, p);
    }
    return result;
}

void addColliders(const std::vector<gps::BoundingBox>& boxes, const glm::mat4& m) {
    for (const gps::BoundingBox& box : boxes) {
        worldColliders.push_back(transformBox(box, m));
    }
}

void updateColliders() {
    worldColliders.clear();
    addColliders(satuMareBounds, satuMareMatrix());
    addColliders(genexBounds, genexMatrix());
    addColliders(restaurantBounds, restaurantMatrix());
    addColliders(panoramaBounds, panoramaMatrix());
    addColliders(telefoaneBounds, telefoaneMatrix());
    addColliders(motherlandBounds, glm::translate(glm::mat4(1.0f), MOTHERLAND_POS));
    addColliders(romanitaBounds, glm::translate(glm::mat4(1.0f), ROMANITA_POS));
    addColliders(powerPlantBounds, glm::translate(glm::mat4(1.0f), POWER_PLANT_POS));
    addColliders(dugaBounds, glm::translate(glm::mat4(1.0f), DUGA_POS));
    addColliders(pripyatBounds, glm::translate(glm::mat4(1.0f), PRIPYAT_POS));
    addColliders(ferrisSupportBounds, glm::translate(glm::mat4(1.0f), FERRIS_POS));
    addColliders(ferrisWheelBounds, ferrisWheelMatrix());
    addColliders(cityNorthBounds, glm::mat4(1.0f));
    addColliders(citySouthBounds, glm::mat4(1.0f));
    for (const glm::vec3& base : LAMP_POSITIONS) {
        addColliders(lampPostBounds, glm::translate(glm::mat4(1.0f), base));
    }
    for (const SpotLight& spot : SPOT_LIGHTS) {
        addColliders(floodlightBounds, glm::translate(glm::mat4(1.0f), spot.position));
    }
}

// ground height under (x, z): bilinear interpolation between the 4 nearest samples
float groundHeight(float x, float z) {
    const HeightMap& m = terrainHeights;
    if (m.heights.empty()) return 0.0f;
    float fx = glm::clamp((x - m.x0) / m.step, 0.0f, (float)(m.nx - 1) - 0.001f);
    float fz = glm::clamp((z - m.z0) / m.step, 0.0f, (float)(m.nz - 1) - 0.001f);
    int i = (int)fx, j = (int)fz;
    float tx = fx - i, tz = fz - j;
    float h00 = m.heights[j * m.nx + i], h10 = m.heights[j * m.nx + i + 1];
    float h01 = m.heights[(j + 1) * m.nx + i], h11 = m.heights[(j + 1) * m.nx + i + 1];
    return glm::mix(glm::mix(h00, h10, tx), glm::mix(h01, h11, tx), tz);
}

// sphere vs box: the closest point of the box to the sphere center must be farther than the radius
bool collides(const glm::vec3& center) {
    for (const gps::BoundingBox& box : worldColliders) {
        glm::vec3 closest = glm::clamp(center, box.min, box.max);
        glm::vec3 d = center - closest;
        if (glm::dot(d, d) < CAMERA_RADIUS * CAMERA_RADIUS) {
            return true;
        }
    }
    return false;
}

// moves the camera, but stops it at walls; the free components of the move are kept,
// so the camera slides along a wall instead of getting stuck on it
void moveCamera(gps::MOVE_DIRECTION direction, float distance) {
    glm::vec3 oldPos = myCamera.getPosition();
    myCamera.move(direction, distance);
    if (!collisionsEnabled) return;

    glm::vec3 newPos = myCamera.getPosition();
    newPos.y = glm::max(newPos.y, groundHeight(newPos.x, newPos.z) + CAMERA_MIN_HEIGHT);
    if (!collides(newPos) || collides(oldPos)) { // already inside (e.g. collisions just turned on): let it out
        myCamera.setPosition(newPos);
        return;
    }

    glm::vec3 pos = oldPos;
    glm::vec3 delta = newPos - oldPos;
    for (int axis = 0; axis < 3; axis++) {
        glm::vec3 attempt = pos;
        attempt[axis] += delta[axis];
        if (!collides(attempt)) {
            pos = attempt;
        }
    }
    myCamera.setPosition(pos);
}
