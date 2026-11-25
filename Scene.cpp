#include "Scene.hpp"
#include "Lighting.hpp"   // the street lamps and the floodlights of the layout


#include <fstream>
#include <algorithm>
#include <cfloat>
#include <random>
#include <sstream>

// ==============================================================================================================
// Scene
// ==============================================================================================================

// models
gps::Model3D terrain;
gps::Model3D water;     // river and cooling pond, drawn with moving waves
gps::Model3D satuMare;
gps::Model3D genex;
gps::Model3D genexRestaurant;
gps::Model3D genexLights;
gps::Model3D romanitaLights, telefoaneLights, satuMareLights;   // lit windows, drawn (glowing) only at night      // lit windows of Genex, drawn (glowing) only at night
gps::Model3D panorama;
gps::Model3D telefoane;
gps::Model3D floodlight;
gps::Model3D floodlightLens;   // only the lenses glow at night
gps::Model3D lampPost;
gps::Model3D lampBulb;
gps::Model3D cityNorth, citySouth, cityProps, cityLights;
std::vector<CityProp> cityProps_;
const VehicleType VEHICLES[VEHICLE_TYPES] = {
    { "models/cars/car_red.obj", "models/cars/car_lights.obj", 4.35f, false, 3, true },
    { "models/cars/car_blue.obj", "models/cars/car_lights.obj", 4.35f, false, 3, true },
    { "models/cars/car_white.obj", "models/cars/car_lights.obj", 4.35f, false, 3, true },
    { "models/cars/car_yellow.obj", "models/cars/car_lights.obj", 4.35f, false, 3, true },
    { "models/vehicles/vaz2101/vaz2101.obj", "models/vehicles/vaz2101/vaz2101_lights.obj", 4.07f, true, 8, true },
    { "models/vehicles/vaz2103/vaz2103.obj", "models/vehicles/vaz2103/vaz2103_lights.obj", 4.12f, true, 6, true },
    { "models/vehicles/vaz2104/vaz2104.obj", "models/vehicles/vaz2104/vaz2104_lights.obj", 4.12f, true, 6, true },
    { "models/vehicles/vaz2105/vaz2105.obj", "models/vehicles/vaz2105/vaz2105_lights.obj", 4.13f, true, 8, true },
    { "models/vehicles/vaz2106/vaz2106.obj", "models/vehicles/vaz2106/vaz2106_lights.obj", 4.17f, true, 8, true },
    { "models/vehicles/vaz2107/vaz2107.obj", "models/vehicles/vaz2107/vaz2107_lights.obj", 4.15f, true, 8, true },
    { "models/vehicles/vaz2107_taxi/vaz2107_taxi.obj", "models/vehicles/vaz2107_taxi/vaz2107_taxi_lights.obj", 4.15f, false, 3, true },
    { "models/vehicles/moskvich2141/moskvich2141.obj", "models/vehicles/moskvich2141/moskvich2141_lights.obj", 4.35f, true, 7, true },
    { "models/vehicles/vis2345/vis2345.obj", "models/vehicles/vis2345/vis2345_lights.obj", 4.4f, true, 3, true },
    { "models/vehicles/gaz52/gaz52.obj", "models/vehicles/gaz52/gaz52_lights.obj", 6.4f, false, 3, false },
    { "models/vehicles/ural4320/ural4320.obj", "models/vehicles/ural4320/ural4320_lights.obj", 7.37f, false, 2, false },
    { "models/vehicles/laz699/laz699.obj", "models/vehicles/laz699/laz699_lights.obj", 10.2f, false, 3, false },
};
gps::Model3D vehicleModels[VEHICLE_TYPES];
gps::Model3D vehicleLights[VEHICLE_TYPES];

// the usual car colors of the '70s-'80s (sRGB): white, beige, red, cherry, navy, sky blue, green, olive, mustard,
// orange, grey-blue, coffee, black
glm::vec3 vehiclePaint(unsigned int seed) {
    static const unsigned char colors[][3] = {
        { 235, 235, 228 }, { 214, 196, 150 }, { 160, 30, 25 }, { 100, 20, 30 }, { 30, 45, 90 }, { 110, 160, 200 },
        { 50, 110, 70 }, { 110, 115, 60 }, { 210, 160, 40 }, { 215, 100, 30 }, { 120, 140, 150 }, { 110, 70, 45 },
        { 25, 25, 28 } };
    const unsigned char* c = colors[(seed * 2654435761u >> 16) % (sizeof(colors) / sizeof(colors[0]))];
    return glm::pow(glm::vec3(c[0], c[1], c[2]) / 255.0f, glm::vec3(2.2f));
}
gps::Model3D motherland, romanita, powerPlant, duga, pripyatBlocks;
gps::Model3D motherlandLod, powerPlantLod, dugaLod, pripyatBlocksLod;
gps::Model3D ferrisSupport, ferrisWheel, ferrisGondola;
gps::Model3D trees;
const char* TREE_SPECIES[] = { "fir", "pine", "noble", "dead", "dead2" };
gps::Model3D treeModels[TREE_SPECIES_COUNT];
std::vector<NearTree> nearTrees;
gps::Model3D detailObjects, detailDecals;
std::vector<gps::BoundingBox> detailObjectBounds, detailDecalBounds;

// animations
float restaurantAngle = 0.0f;
const float RESTAURANT_SPEED = 6.0f;
const float FERRIS_SPEED = 4.0f;         // degrees per second
float ferrisAngle = 0.0f;

// keyboard-controlled object (Palatul Telefoanelor)
Placement satuMarePlace{ { 0.0f, 0.0f, 60.0f }, 0.0f };
Placement genexPlace{ { -420.0f, 0.0f, 196.0f }, 0.0f };
Placement panoramaPlace{ { -100.0f, 70.0f, -520.0f }, 0.0f };
Placement motherlandPlace{ { 450.0f, 35.0f, -150.0f }, 0.0f };
Placement romanitaPlace{ { 200.0f, 0.0f, 280.0f }, 0.0f };
Placement powerPlantPlace{ { -780.0f, 0.0f, 70.0f }, 0.0f };
Placement dugaPlace{ { -700.0f, 0.0f, -330.0f }, 0.0f };
Placement pripyatPlace{ { -780.0f, 0.0f, 250.0f }, 0.0f };
Placement ferrisPlace{ { -640.0f, 0.0f, 320.0f }, 0.0f };
Placement telefoanePlace{ { -170.0f, 0.0f, 140.0f }, 0.0f };
glm::vec3 telefoanePos = telefoanePlace.pos;
float telefoaneAngle = 0.0f;   // degrees, around its own vertical axis
float telefoaneScale = 1.0f;   // uniform scale

// street lamps and parked cars of the town: "lamp x y z yaw" or "car <color> x y z yaw"
bool loadCityProps(const char* path) {
    std::ifstream in(path);
    if (!in) {
        std::cerr << "Could not open " << path << std::endl;
        return false;
    }
    std::string kind;
    while (in >> kind) {
        if (kind[0] == '#') { std::getline(in, kind); continue; }   // comment line
        int car = -1;
        if (kind == "car") in >> car;
        float x, y, z, yaw;
        in >> x >> y >> z >> yaw;
        glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, z));
        // a parked car: one of the vehicles that park, chosen from its place (always the same)
        unsigned int seed = (unsigned int)(fabs(x) * 7.0f) * 73856093u ^ (unsigned int)(fabs(z) * 7.0f) * 19349663u;
        if (car >= 0) {
            std::vector<int> parking;
            for (int i = 0; i < VEHICLE_TYPES; i++) if (VEHICLES[i].parks) parking.push_back(i);
            car = parking[(seed >> 3) % parking.size()];
        }
        cityProps_.push_back({ car, glm::rotate(m, glm::radians(yaw), glm::vec3(0.0f, 1.0f, 0.0f)), vehiclePaint(seed) });
    }
    return true;
}

// one line per tree: species x y z yaw(degrees) scale
bool loadNearTrees(const char* path) {
    std::ifstream in(path);
    if (!in) {
        std::cerr << "Could not open " << path << std::endl;
        return false;
    }
    std::string name;
    while (in >> name) {
        if (name[0] == '#') { std::getline(in, name); continue; }   // comment line
        float x, y, z, yaw, scale;
        in >> x >> y >> z >> yaw >> scale;
        for (int i = 0; i < TREE_SPECIES_COUNT; i++) {
            if (name == TREE_SPECIES[i]) {
                glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, z));
                m = glm::rotate(m, glm::radians(yaw), glm::vec3(0.0f, 1.0f, 0.0f));
                nearTrees.push_back({ i, glm::scale(m, glm::vec3(scale)) });
            }
        }
    }
    return true;
}

glm::mat4 placed(const Placement& p) {
    glm::mat4 m = glm::translate(glm::mat4(1.0f), p.pos);
    return glm::rotate(m, glm::radians(p.yaw), glm::vec3(0.0f, 1.0f, 0.0f));
}

// one line per object: <name> x y z yaw  |  lamp x y z  |  floodlight x y z  target_x target_y target_z
void loadLayout(const char* path) {
    std::ifstream in(path);
    if (!in) {
        std::cerr << "Could not open " << path << ", the default layout is used" << std::endl;
        return;
    }
    std::pair<const char*, Placement*> places[] = {
        { "satu_mare", &satuMarePlace }, { "genex", &genexPlace }, { "panorama", &panoramaPlace },
        { "motherland", &motherlandPlace }, { "romanita", &romanitaPlace }, { "power_plant", &powerPlantPlace },
        { "duga", &dugaPlace }, { "pripyat", &pripyatPlace }, { "ferris_wheel", &ferrisPlace },
        { "telefoane", &telefoanePlace } };
    std::vector<glm::vec3> lamps;
    std::vector<SpotLight> spots;
    std::string name;
    while (in >> name) {
        if (name[0] == '#') { std::getline(in, name); continue; }   // comment line
        glm::vec3 p;
        in >> p.x >> p.y >> p.z;
        if (name == "lamp") {
            lamps.push_back(p);
        } else if (name == "floodlight") {
            glm::vec3 target;
            in >> target.x >> target.y >> target.z;
            spots.push_back({ p, target });
        } else {
            float yaw;
            in >> yaw;
            bool known = false;
            for (auto& place : places) {
                if (name == place.first) { *place.second = { p, yaw }; known = true; }
            }
            if (!known) std::cerr << path << ": unknown object " << name << std::endl;
        }
    }
    if (!lamps.empty()) LAMP_POSITIONS = lamps;
    if (!spots.empty()) SPOT_LIGHTS = spots;
    resetTelefoane();
}

void initModels() {
    loadLayout("models/layout.txt");
    terrain.LoadModel("models/terrain/terrain.obj");
    water.LoadModel("models/terrain/water.obj");
    satuMare.LoadModel("models/satu_mare/satu_mare.obj");
    genex.LoadModel("models/genex/genex.obj");
    genexRestaurant.LoadModel("models/genex/genex_restaurant.obj");
    genexLights.LoadModel("models/genex/genex_lights.obj");
    romanitaLights.LoadModel("models/romanita/romanita_lights.obj");
    telefoaneLights.LoadModel("models/telefoane/telefoane_lights.obj");
    satuMareLights.LoadModel("models/satu_mare/satu_mare_lights.obj");
    panorama.LoadModel("models/panorama/panorama.obj");
    telefoane.LoadModel("models/telefoane/telefoane.obj");
    lampPost.LoadModel("models/lamp_post/lamp_post.obj");
    lampBulb.LoadModel("models/lamp_post/lamp_bulb.obj");
    floodlight.LoadModel("models/floodlight/floodlight.obj");
    floodlightLens.LoadModel("models/floodlight/floodlight_lens.obj");
    for (int i = 0; i < VEHICLE_TYPES; i++) {
        vehicleModels[i].LoadModel(VEHICLES[i].model);
        vehicleLights[i].LoadModel(VEHICLES[i].lights);
    }
    motherland.LoadModel("models/motherland/motherland.obj");
    romanita.LoadModel("models/romanita/romanita.obj");
    powerPlant.LoadModel("models/power_plant/power_plant.obj");
    duga.LoadModel("models/duga/duga.obj");
    pripyatBlocks.LoadModel("models/pripyat/pripyat_blocks.obj");
    motherlandLod.LoadModel("models/motherland/motherland_lod.obj");
    powerPlantLod.LoadModel("models/power_plant/power_plant_lod.obj");
    dugaLod.LoadModel("models/duga/duga_lod.obj");
    pripyatBlocksLod.LoadModel("models/pripyat/pripyat_blocks_lod.obj");
    ferrisSupport.LoadModel("models/ferris_wheel/ferris_support.obj");
    ferrisWheel.LoadModel("models/ferris_wheel/ferris_wheel.obj");
    ferrisGondola.LoadModel("models/ferris_wheel/ferris_gondola.obj");
    trees.LoadModel("models/trees/trees.obj");
    for (int i = 0; i < TREE_SPECIES_COUNT; i++) {
        treeModels[i].LoadModel(std::string("models/trees/tree_") + TREE_SPECIES[i] + ".obj");
    }
    loadNearTrees("models/trees/trees_near.txt");
    cityNorth.LoadModel("models/city/city_north.obj");
    citySouth.LoadModel("models/city/city_south.obj");
    cityProps.LoadModel("models/city/city_props.obj");
    cityLights.LoadModel("models/city/city_lights.obj");
    loadCityProps("models/city/props.txt");
    detailObjects.LoadModel("models/details/objects.obj");
    detailDecals.LoadModel("models/details/decals.obj");
    detailObjectBounds = detailObjects.GetMeshBounds();
    detailDecalBounds = detailDecals.GetMeshBounds();
}

void updateAnimations() {
    restaurantAngle = fmod(restaurantAngle + RESTAURANT_SPEED * deltaTime, 360.0f);
    ferrisAngle = fmod(ferrisAngle + FERRIS_SPEED * deltaTime, 360.0f);
}

void resetTelefoane() {
    telefoanePos = telefoanePlace.pos;
    telefoaneAngle = telefoanePlace.yaw;
    telefoaneScale = 1.0f;
}

// model matrices, shared by rendering and collision detection
glm::mat4 satuMareMatrix() { return placed(satuMarePlace); }
glm::mat4 genexMatrix() { return placed(genexPlace); }
glm::mat4 panoramaMatrix() { return placed(panoramaPlace); }

glm::mat4 restaurantMatrix() {
    // rotate around its own axis first, then move it on top of the tower (T * R)
    glm::mat4 m = glm::translate(placed(genexPlace), RESTAURANT_OFFSET);
    return glm::rotate(m, glm::radians(restaurantAngle), glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 ferrisWheelMatrix() {
    // the wheel turns around its horizontal axle (Z), at the height of the hub
    glm::mat4 m = glm::translate(placed(ferrisPlace), glm::vec3(0.0f, FERRIS_HUB_HEIGHT, 0.0f));
    return glm::rotate(m, glm::radians(ferrisAngle), glm::vec3(0.0f, 0.0f, 1.0f));
}

// the gondolas hang from the rim: they follow the wheel, but only by translation, so they stay upright
glm::mat4 ferrisGondolaMatrix(int i) {
    float angle = glm::radians(ferrisAngle) + glm::two_pi<float>() * i / FERRIS_GONDOLAS;
    glm::vec3 offset = glm::vec3(0.0f, FERRIS_HUB_HEIGHT, 0.0f) + FERRIS_RADIUS * glm::vec3(cos(angle), sin(angle), 0.0f);
    return glm::translate(placed(ferrisPlace), offset);
}

glm::mat4 telefoaneMatrix() {
    // T * R * S: scale and rotate around the building's own base, then move it into place
    glm::mat4 m = glm::translate(glm::mat4(1.0f), telefoanePos);
    m = glm::rotate(m, glm::radians(telefoaneAngle), glm::vec3(0.0f, 1.0f, 0.0f));
    return glm::scale(m, glm::vec3(telefoaneScale));
}

// ==============================================================================================================
// Traffic
// ==============================================================================================================

const float CAR_GAP = 2.5f;                       // meters between two cars that stand in a queue
bool trafficEnabled = true;
int trafficCars = 200;                            // how many cars drive on the roads
std::vector<TrafficLane> trafficLanes;
std::vector<Car> cars;
std::mt19937 trafficRng(7);

bool loadTraffic(const char* path) {
    std::ifstream in(path);
    if (!in) {
        std::cerr << "Cannot open " << path << std::endl;
        return false;
    }
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream words(line);
        std::string tag;
        words >> tag;
        if (tag == "lane") {
            int id, junction, n;
            float speed;
            words >> id >> junction >> speed >> n;
            if ((int)trafficLanes.size() <= id) trafficLanes.resize(id + 1);
            TrafficLane& lane = trafficLanes[id];
            lane.junction = junction;
            lane.speed = speed;
            for (int i = 0; i < n; i++) {
                glm::vec3 p;
                words >> p.x >> p.y >> p.z;
                lane.distance.push_back(lane.points.empty() ? 0.0f : lane.distance.back() + glm::distance(lane.points.back(), p));
                lane.points.push_back(p);
            }
            lane.length = lane.distance.back();
        } else if (tag == "next" || tag == "conf") {
            int id, k, other;
            words >> id >> k;
            std::vector<int>& list = tag == "next" ? trafficLanes[id].next : trafficLanes[id].conflicts;
            for (int i = 0; i < k; i++) {
                words >> other;
                list.push_back(other);
            }
        }
    }
    return !trafficLanes.empty();
}

int pickNextLane(int lane) {
    const std::vector<int>& next = trafficLanes[lane].next;
    std::uniform_int_distribution<int> pick(0, (int)next.size() - 1);
    return next[pick(trafficRng)];
}

// the cars are spread over the lanes (not the junctions), at least 12 m apart
void initTraffic() {
    cars.clear();
    for (TrafficLane& lane : trafficLanes) lane.occupants = 0;
    std::vector<int> roads;
    std::vector<float> weights;
    for (int i = 0; i < (int)trafficLanes.size(); i++) {
        if (trafficLanes[i].junction < 0 && trafficLanes[i].length > 20.0f) {
            roads.push_back(i);
            weights.push_back(trafficLanes[i].length);
        }
    }
    if (roads.empty()) return;
    std::discrete_distribution<int> pickLane(weights.begin(), weights.end());
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    std::vector<int> weights_;
    for (int i = 0; i < VEHICLE_TYPES; i++) weights_.push_back(VEHICLES[i].trafficWeight);
    std::discrete_distribution<int> pickType(weights_.begin(), weights_.end());
    for (int attempt = 0; attempt < trafficCars * 20 && (int)cars.size() < trafficCars; attempt++) {
        int lane = roads[pickLane(trafficRng)];
        int type = pickType(trafficRng);
        float length = VEHICLES[type].length;
        float s = 0.5f * length + unit(trafficRng) * (trafficLanes[lane].length - length);
        bool free = true;
        for (const Car& car : cars) {
            if (car.lane == lane && fabs(car.s - s) < 8.0f + 0.5f * (length + VEHICLES[car.model].length)) free = false;
        }
        if (!free) continue;
        float eagerness = 0.85f + 0.25f * unit(trafficRng);
        // the trucks and the buses are slower
        if (length > 6.0f) eagerness *= 0.8f;
        cars.push_back(Car{ lane, s, trafficLanes[lane].speed * eagerness, eagerness, pickNextLane(lane),
                            type, vehiclePaint((unsigned int)trafficRng()), 0.0f });
        trafficLanes[lane].occupants++;
    }
}

// a junction curve can be entered if nobody is on a curve that crosses it and there is room after it
bool junctionFree(int lane) {
    const TrafficLane& curve = trafficLanes[lane];
    for (int other : curve.conflicts) {
        if (trafficLanes[other].occupants > 0) return false;
    }
    return true;
}

void updateTraffic() {
    if (!trafficEnabled || trafficLanes.empty()) return;
    float dt = glm::min(deltaTime, 0.1f);
    // the cars of each lane, in the order they drive
    std::vector<std::vector<int>> onLane(trafficLanes.size());
    for (int i = 0; i < (int)cars.size(); i++) onLane[cars[i].lane].push_back(i);
    for (std::vector<int>& list : onLane) {
        std::sort(list.begin(), list.end(), [](int a, int b) { return cars[a].s < cars[b].s; });
    }
    // the vehicles are placed by their middle: half of their length in front of it and behind it
    auto half = [&](int i) { return 0.5f * VEHICLES[cars[i].model].length; };
    // the rear of the last car on a lane (from the lane's start); FLT_MAX if the lane is empty
    auto firstCar = [&](int lane) {
        return onLane[lane].empty() ? FLT_MAX : cars[onLane[lane].front()].s - half(onLane[lane].front());
    };
    for (int i = 0; i < (int)cars.size(); i++) {
        Car& car = cars[i];
        const TrafficLane& lane = trafficLanes[car.lane];
        float toEnd = lane.length - car.s;
        // the obstacle in front: the next car on this lane, or the last car on the next lane
        float gap = FLT_MAX;
        const std::vector<int>& here = onLane[car.lane];
        auto it = std::find(here.begin(), here.end(), i);
        if (it + 1 != here.end()) {
            gap = cars[*(it + 1)].s - half(*(it + 1)) - car.s - half(i);
        } else {
            gap = toEnd + firstCar(car.next) - half(i);
            // a junction curve: also the car after it, so nobody stops in the middle of the junction
            const TrafficLane& next = trafficLanes[car.next];
            if (next.junction >= 0 && !next.next.empty()) {
                gap = glm::min(gap, toEnd + next.length + firstCar(next.next[0]) - half(i));
                bool blocked = !junctionFree(car.next) ||
                               firstCar(next.next[0]) < 2.0f * half(i) + CAR_GAP;
                if (lane.junction < 0 && blocked && car.waited < 8.0f) gap = glm::min(gap, toEnd - half(i) + 1.0f);
            }
        }
        float target = glm::min(lane.speed * car.eagerness, sqrtf(glm::max(0.0f, 2.0f * 5.0f * (gap - CAR_GAP))));
        car.speed = car.speed < target ? glm::min(target, car.speed + 3.0f * dt) : glm::max(target, car.speed - 9.0f * dt);
        car.waited = (car.speed < 0.3f && toEnd - half(i) < 4.0f) ? car.waited + dt : 0.0f;
        car.s += car.speed * dt;
        // on into the next lane (possibly more than one if the lane is very short)
        while (car.s >= trafficLanes[car.lane].length) {
            car.s -= trafficLanes[car.lane].length;
            trafficLanes[car.lane].occupants--;
            car.lane = car.next;
            trafficLanes[car.lane].occupants++;
            car.next = pickNextLane(car.lane);
        }
    }
}

// position and orientation of a car on its lane (it faces +X in model space)
glm::mat4 carMatrix(const Car& car) {
    const TrafficLane& lane = trafficLanes[car.lane];
    int i = (int)(std::upper_bound(lane.distance.begin(), lane.distance.end(), car.s) - lane.distance.begin()) - 1;
    i = glm::clamp(i, 0, (int)lane.points.size() - 2);
    float t = glm::clamp((car.s - lane.distance[i]) / glm::max(lane.distance[i + 1] - lane.distance[i], 1e-4f), 0.0f, 1.0f);
    glm::vec3 position = glm::mix(lane.points[i], lane.points[i + 1], t);
    glm::vec3 forward = glm::normalize(lane.points[i + 1] - lane.points[i]);
    glm::vec3 side = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
    glm::vec3 up = glm::cross(side, forward);
    glm::mat4 m(1.0f);
    m[0] = glm::vec4(forward, 0.0f);
    m[1] = glm::vec4(up, 0.0f);
    m[2] = glm::vec4(side, 0.0f);
    m[3] = glm::vec4(position, 1.0f);
    return m;
}
