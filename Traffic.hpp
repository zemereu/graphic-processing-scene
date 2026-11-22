// Traffic on all the roads (models/city/traffic.txt, written by tools/traffic_net.py): every road is split into
// lanes (right-hand traffic) and the lanes are joined through the junctions, the roundabout, the bridge and the
// turning places by short curves. A car follows its lane, keeps its distance to the car in front and at the end of
// the lane goes on into one of the next lanes, chosen at random; it waits in front of a junction while another car
// is on a curve that crosses its own.
#pragma once

#include "App.hpp"

struct TrafficLane {
    std::vector<glm::vec3> points;
    std::vector<float> distance;    // distance along the lane at each point
    float length = 0.0f;
    float speed = 10.0f;            // speed limit, m/s
    int junction = -1;              // >= 0: a curve through a junction
    std::vector<int> next;          // the lanes a car can go on into
    std::vector<int> conflicts;     // the curves of the same junction that cross this one
    int occupants = 0;              // cars on this lane now
};
struct Car {
    int lane;          // the lane the car is on
    float s;           // distance along the lane
    float speed;       // m/s
    float eagerness;   // multiplies the speed limit
    int next;          // the lane after this one (chosen when the car enters the lane)
    int model;         // index in carModels (body color)
    float waited;      // seconds spent waiting in front of a junction
};
extern bool trafficEnabled;
extern int trafficCars;                            // how many cars drive on the roads
extern std::vector<TrafficLane> trafficLanes;
extern std::vector<Car> cars;

bool loadTraffic(const char* path);
void initTraffic();     // spreads the cars over the lanes (again, when their number changes)
void updateTraffic();   // every frame
glm::mat4 carMatrix(const Car& car);
