#pragma once

#include <cstdint>
#include "experiments/csharp/CSharpHostTypes.h"

namespace bl::csharp {

struct BenchmarkResult {
    double cppNsPerCall = 0.0;
    double csharpNsPerCall = 0.0;
    double ratio = 0.0;
    int64_t cppValue = 0;
    int64_t csharpValue = 0;
};

bool installPrototype();
bool available();
int version();
int hostApiVersion();
int add(int a, int b);
int64_t checksum(int iterations);
int64_t playerSample(float x, float y, int life, int maxLife);
uint32_t mathBatch(int iterations, uint32_t seed);
double projectileBatch(int count, int steps);
double npcScan(const NpcSample* samples, int count, float playerX, float playerY, int repeats);
double projectileScan(const ProjectileSample* samples, int count, int repeats);
bool hostProjectileTick(HostProjectileState* state, const NpcSample* samples, int count,
                        float playerX, float playerY);
double clockMs();
BenchmarkResult benchmark(int iterations);

}
