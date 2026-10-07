#include <mod/amlmod.h>
#include <mod/logger.h>
#include <math.h>
#include <time.h>

MYMOD(net.user.driftcam, DriftCam, 1.0, You)

struct Vec3 { float x, y, z; };

static const float MIN_SPEED   = 6.0f;
static const float MIN_ANGLE   = 0.12f;
static const float MAX_ANGLE   = 1.40f;
static const float STRENGTH    = 0.85f;
static const float SMOOTHNESS  = 6.0f;

typedef void (*FollowCar_t)(void*, const Vec3&, float, float, float, bool);
static FollowCar_t orig = nullptr;

static Vec3  lastPos{};
static bool  hasLast = false;
static float lastT = 0.0f, offsetAngle = 0.0f;

static float Now() {
    timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9f;
}
static float Wrap(float a) {
    while (a >  M_PI) a -= 2 * M_PI;
    while (a < -M_PI) a += 2 * M_PI;
    return a;
}

static void Hooked(void* self, const Vec3& pos, float heading, float a, float b, bool c) {
    float t = Now(), dt = t - lastT;
    float want = 0.0f;

    if (hasLast && dt >
