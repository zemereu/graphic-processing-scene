#include "Traffic.hpp"
#include "Scene.hpp"   // CAR_COLORS

#include <algorithm>
#include <cfloat>
#include <fstream>
#include <random>
#include <sstream>

const float CAR_LENGTH = 4.4f;
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
    std::uniform_int_distribution<int> color(0, CAR_COLORS - 1);
    for (int attempt = 0; attempt < trafficCars * 20 && (int)cars.size() < trafficCars; attempt++) {
        int lane = roads[pickLane(trafficRng)];
        float s = unit(trafficRng) * (trafficLanes[lane].length - CAR_LENGTH);
        bool free = true;
        for (const Car& car : cars) {
            if (car.lane == lane && fabs(car.s - s) < 12.0f) free = false;
        }
        if (!free) continue;
        float eagerness = 0.85f + 0.25f * unit(trafficRng);
        cars.push_back(Car{ lane, s, trafficLanes[lane].speed * eagerness, eagerness, pickNextLane(lane),
                            color(trafficRng), 0.0f });
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
    // the rear of the last car on a lane (from the lane's start); FLT_MAX if the lane is empty
    auto firstCar = [&](int lane) {
        return onLane[lane].empty() ? FLT_MAX : cars[onLane[lane].front()].s - CAR_LENGTH;
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
            gap = cars[*(it + 1)].s - car.s - CAR_LENGTH;
        } else {
            gap = toEnd + firstCar(car.next);
            // a junction curve: also the car after it, so nobody stops in the middle of the junction
            const TrafficLane& next = trafficLanes[car.next];
            if (next.junction >= 0 && !next.next.empty()) {
                gap = glm::min(gap, toEnd + next.length + firstCar(next.next[0]));
                bool blocked = !junctionFree(car.next) ||
                               firstCar(next.next[0]) < CAR_LENGTH + CAR_GAP;
                if (lane.junction < 0 && blocked && car.waited < 8.0f) gap = glm::min(gap, toEnd - 1.0f);
            }
        }
        float target = glm::min(lane.speed * car.eagerness, sqrtf(glm::max(0.0f, 2.0f * 5.0f * (gap - CAR_GAP))));
        car.speed = car.speed < target ? glm::min(target, car.speed + 3.0f * dt) : glm::max(target, car.speed - 9.0f * dt);
        car.waited = (car.speed < 0.3f && toEnd < 6.0f) ? car.waited + dt : 0.0f;
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
