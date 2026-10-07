#pragma once

#include <cstdint>

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
int add(int a, int b);
int64_t checksum(int iterations);
int64_t playerSample(float x, float y, int life, int maxLife);
BenchmarkResult benchmark(int iterations);

}
