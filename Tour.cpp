#include "Tour.hpp"
#include "Input.hpp"   // yaw, pitch, firstMouse

// camera tour: the camera follows a smooth path through these key points
struct TourKey {
    glm::vec3 position;
    glm::vec3 target;   // point the camera looks at
};
const std::vector<TourKey> TOUR = {
    {{100.0f, 40.0f, 990.0f}, {100.0f, 8.0f, 700.0f}},  // from the road coming in from the south, the roundabout ahead
    {{100.0f, 16.0f, 738.0f}, {100.0f, 12.0f, 660.0f}},  // the monument in the middle of the roundabout
    {{152.0f, 15.0f, 684.0f}, {100.0f, 14.0f, 660.0f}},  // around the roundabout: the curved streets
    {{100.0f, 14.0f, 612.0f}, {100.0f, 12.0f, 300.0f}},  // down the boulevard
    {{128.0f, 20.0f, 330.0f}, {200.0f, 40.0f, 280.0f}},  // Romanita in its park
    {{132.0f, 9.0f, 214.0f}, {0.0f, 35.0f, 70.0f}},  // the crossing with the highway, the Administrative Palace
    {{-30.0f, 6.0f, 150.0f}, {0.0f, 60.0f, 60.0f}},  // look up at the palace from its plaza
    {{92.0f, 10.0f, 118.0f}, {70.0f, 6.0f, 30.0f}},  // along the boulevard, past the palace
    {{62.0f, 9.0f, 24.0f}, {-60.0f, 3.0f, -40.0f}},  // on the quay: the river, the footbridge
    {{-100.0f, 10.0f, 8.0f}, {-170.0f, 10.0f, 140.0f}},  // along the promenade
    {{-118.0f, 15.0f, 214.0f}, {-170.0f, 10.0f, 140.0f}},  // Palatul Telefoanelor
    {{-300.0f, 10.0f, 196.0f}, {-420.0f, 40.0f, 196.0f}},  // west along the highway, the Genex gate
    {{-470.0f, 7.0f, 196.0f}, {-640.0f, 10.0f, 230.0f}},  // the checkpoint, into the exclusion zone
    {{-590.0f, 30.0f, 390.0f}, {-690.0f, 15.0f, 300.0f}},  // Pripyat and the Ferris wheel
    {{-650.0f, 60.0f, 190.0f}, {-780.0f, 30.0f, 70.0f}},  // the power plant
    {{-680.0f, 75.0f, -150.0f}, {-700.0f, 60.0f, -330.0f}},  // over the river, the Duga radar
    {{-300.0f, 110.0f, -330.0f}, {-100.0f, 80.0f, -520.0f}},  // along the north bank, Hotel Panorama in the mountains
    {{260.0f, 75.0f, -70.0f}, {450.0f, 60.0f, -150.0f}},  // Motherland Calls on her hill
    {{560.0f, 70.0f, 40.0f}, {520.0f, 20.0f, 400.0f}},  // back over the river, down the curved avenue in the east
    {{610.0f, 50.0f, 420.0f}, {470.0f, 10.0f, 640.0f}},  // over the eastern housing estate
    {{420.0f, 42.0f, 900.0f}, {100.0f, 15.0f, 700.0f}},  // the new southern district, back towards the roundabout
    {{100.0f, 40.0f, 990.0f}, {100.0f, 8.0f, 700.0f}},  // back to the start
};
const float TOUR_SPEED = 35.0f;       // meters per second along the path
const float TOUR_MIN_SEGMENT = 3.0f;  // seconds, so short segments are not rushed
std::vector<float> tourKeyTimes;      // time at which the camera reaches each key point
bool tourActive = false;
float tourTime = 0.0f;

// Catmull-Rom spline: passes through p1 (t = 0) and p2 (t = 1), p0 and p3 shape the curve
glm::vec3 catmullRom(const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3, float t) {
    float t2 = t * t, t3 = t2 * t;
    return 0.5f * ((2.0f * p1) + (-p0 + p2) * t + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2
        + (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
}

void initTour() {
    // each segment lasts proportionally to its length -> roughly constant speed
    tourKeyTimes = { 0.0f };
    for (size_t i = 0; i + 1 < TOUR.size(); i++) {
        float length = glm::distance(TOUR[i].position, TOUR[i + 1].position);
        tourKeyTimes.push_back(tourKeyTimes.back() + glm::max(TOUR_MIN_SEGMENT, length / TOUR_SPEED));
    }
}


void startTour() {
    tourActive = true;
    tourTime = 0.0f;
}

void stopTour() {
    tourActive = false;
    // continue with mouse/keyboard from where the tour left the camera, without a jump
    glm::vec3 front = myCamera.getFrontDirection();
    yaw = glm::degrees(atan2(front.z, front.x));
    pitch = glm::degrees(asin(front.y));
    firstMouse = true;
}

void updateTour() {
    if (!tourActive) return;

    tourTime += deltaTime;
    if (tourTime >= tourKeyTimes.back()) {
        myCamera.setLookAt(TOUR.back().position, TOUR.back().target);
        uploadView();
        stopTour();
        return;
    }

    // current segment i: between key i and key i + 1
    int i = 0;
    while (tourKeyTimes[i + 1] < tourTime) i++;
    float t = (tourTime - tourKeyTimes[i]) / (tourKeyTimes[i + 1] - tourKeyTimes[i]);

    auto key = [](int k) { return TOUR[glm::clamp(k, 0, (int)TOUR.size() - 1)]; };
    glm::vec3 position = catmullRom(key(i - 1).position, key(i).position, key(i + 1).position, key(i + 2).position, t);
    glm::vec3 target = catmullRom(key(i - 1).target, key(i).target, key(i + 1).target, key(i + 2).target, t);

    myCamera.setLookAt(position, target);
    uploadView();
}
