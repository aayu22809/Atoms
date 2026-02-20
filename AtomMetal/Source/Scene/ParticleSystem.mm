#import "ParticleSystem.h"
#include "CDFSampler.h"
#include "WaveFunction.h"
#include "ProbabilityCurrent.h"
#include <random>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static simd_float4 heatmapFire(float intensity) {
    intensity = fmax(0.0f, fmin(1.0f, intensity));
    const simd_float4 stops[6] = {
        {0.0f, 0.0f, 0.0f, 1.0f},
        {0.5f, 0.0f, 0.99f, 1.0f},
        {0.8f, 0.0f, 0.0f, 1.0f},
        {1.0f, 0.5f, 0.0f, 1.0f},
        {1.0f, 1.0f, 0.0f, 1.0f},
        {1.0f, 1.0f, 1.0f, 1.0f}
    };
    float scaled = intensity * 5.0f;
    int i = (int)scaled;
    float t = scaled - i;
    i = std::min(i, 4);
    return simd_mix(stops[i], stops[i+1], simd_make_float4(t, t, t, 0.0f));
}

static simd_float4 infernoLog(double intensity) {
    double t = log10(intensity + 1e-12) + 12.0;
    t /= 12.0;
    t = fmax(0.0, fmin(1.0, t));
    auto smoothstep = [](float edge0, float edge1, float x) {
        float t = (x - edge0) / (edge1 - edge0);
        t = fmax(0.0f, fmin(1.0f, t));
        return t * t * (3.0f - 2.0f * t);
    };
    float rC = smoothstep(0.15f, 1.0f, (float)t);
    float gC = smoothstep(0.45f, 1.0f, (float)t);
    float bC = smoothstep(0.85f, 1.0f, (float)t) * 0.2f;
    return simd_make_float4(rC, gC * 0.8f, bC, 1.0f);
}

void generateParticles(int n, int l, int m, int count,
                      std::vector<Particle>& out,
                      std::unordered_map<uint64_t, CDFCache>& cacheMap,
                      std::mt19937& gen,
                      int colorMode) {
    out.clear();
    out.reserve(count);
    for (int i = 0; i < count; ++i) {
        double r = sampleR(n, l, gen, cacheMap);
        double theta = sampleTheta(l, m, gen, cacheMap);
        float phi = samplePhi(gen);
        simd_float3 pos = simd_make_float3(
            (float)(r * sin(theta) * cos(phi)),
            (float)(r * cos(theta)),
            (float)(r * sin(theta) * sin(phi))
        );
        double intensity = probabilityDensity(n, l, m, r, theta);
        simd_float4 color;
        if (colorMode == 0) {
            float scaledIntensity = (float)(intensity * 1.5 * pow(5.0, n));
            scaledIntensity = fmin(1.0f, scaledIntensity * 0.5f);
            color = heatmapFire(scaledIntensity);
        } else {
            color = infernoLog(intensity);
        }
        out.push_back({pos, simd_make_float3(0,0,0), color});
    }
}

void updateParticles(std::vector<Particle>& particles, int m, float dt) {
    for (auto& p : particles) {
        updateParticle(p, m, dt);
    }
}
