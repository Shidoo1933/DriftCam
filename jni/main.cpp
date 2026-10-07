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
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9f;
}

static float Wrap(float a) {
    while (a > M_PI) a -= 2 * M_PI;
    while (a < -M_PI) a += 2 * M_PI;
    return a;
}

static void Hooked(void* self, const Vec3& pos, float heading, float a, float b, bool c) {
    float t = Now();
    float dt = t - lastT;
    float want = 0.0f;

    bool dtOk = hasLast;
    if (dt <= 0.0001f) dtOk = false;
    if (dt >= 0.25f) dtOk = false;

    if (dtOk) {
        float vx = (pos.x - lastPos.x) / dt;
        float vy = (pos.y - lastPos.y) / dt;
        float speed = sqrtf(vx * vx + vy * vy);

        bool speedOk = true;
        if (speed <= MIN_SPEED) speedOk = false;
        if (speed >= 200.0f) speedOk = false;

        if (speedOk) {
            float velHeading = atan2f(-vx, vy);
            float diff = Wrap(velHeading - heading);
            float ad = fabsf(diff);
            bool angleOk = true;
            if (ad <= MIN_ANGLE) angleOk = false;
            if (ad >= MAX_ANGLE) angleOk = false;
            if (angleOk) want = diff * STRENGTH;
        }
    } else {
        offsetAngle = 0.0f;
    }

    float step = 0.016f;
    if (dtOk) step = dt;
    float k = 1.0f - expf(-SMOOTHNESS * step);
    offsetAngle += (want - offsetAngle) * k;

    lastPos = pos;
    lastT = t;
    hasLast = true;
    orig(self, pos, Wrap(heading + offsetAngle), a, b, c);
}

extern "C" void OnModLoad() {
    void* h = aml->GetLibHandle("libGTASA.so");
    const char* names[] = {
        "_ZN4CCam21Process_FollowCar_SAERK7CVectorfffb",
        "_ZN4CCam17Process_FollowCarERK7CVectorfff",
    };
    for (auto n : names) {
        void* f = aml->GetSym(h, n);
        if (f) {
            aml->Hook(f, (void*)Hooked, (void**)&orig);
            logger->Info("DriftCam hooked %s", n);
            return;
        }
    }
    logger->Error("DriftCam: simbol CCam tidak ditemukan");
}
