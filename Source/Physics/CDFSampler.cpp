#include "CDFSampler.h"
#include "WaveFunction.h"
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static uint64_t radialCacheKey(int n, int l) {
    return (uint64_t)(n & 0xFFFF) << 16 | (uint64_t)(l & 0xFF) << 8;
}

static uint64_t thetaCacheKey(int l, int m) {
    return (uint64_t)(l & 0xFF) << 16 | (uint64_t)((m + 128) & 0xFF) << 8;
}

double sampleR(int n, int l, std::mt19937& gen, std::unordered_map<uint64_t, CDFCache>& cacheMap) {
    uint64_t key = radialCacheKey(n, l);
    CDFCache& cache = cacheMap[key];
    if (!cache.built) {
        const int BINS = 4096;
        const double rMax = 10.0 * n * n;
        cache.radialCDF.resize(BINS);
        double dr = rMax / (BINS - 1);
        double sum = 0.0;
        for (int i = 0; i < BINS; ++i) {
            double r = i * dr;
            double R = computeRadial(n, l, r);
            double pdf = r * r * R * R;
            sum += pdf;
            cache.radialCDF[i] = sum;
        }
        for (double& v : cache.radialCDF) v /= sum;
        cache.built = true;
    }
    std::uniform_real_distribution<double> dis(0.0, 1.0);
    double u = dis(gen);
    int idx = (int)(std::lower_bound(cache.radialCDF.begin(), cache.radialCDF.end(), u) - cache.radialCDF.begin());
    const double rMax = 10.0 * n * n;
    return idx * (rMax / 4095.0);
}

double sampleTheta(int l, int m, std::mt19937& gen, std::unordered_map<uint64_t, CDFCache>& cacheMap) {
    uint64_t key = thetaCacheKey(l, m);
    CDFCache& cache = cacheMap[key];
    if (cache.thetaCDF.empty()) {
        const int BINS = 2048;
        cache.thetaCDF.resize(BINS);
        double dtheta = M_PI / (BINS - 1);
        double sum = 0.0;
        int absM = std::abs(m);
        for (int i = 0; i < BINS; ++i) {
            double theta = i * dtheta;
            double Plm = computeLegendre(l, absM, cos(theta));
            double pdf = sin(theta) * Plm * Plm;
            sum += pdf;
            cache.thetaCDF[i] = sum;
        }
        for (double& v : cache.thetaCDF) v /= sum;
    }
    std::uniform_real_distribution<double> dis(0.0, 1.0);
    double u = dis(gen);
    int idx = (int)(std::lower_bound(cache.thetaCDF.begin(), cache.thetaCDF.end(), u) - cache.thetaCDF.begin());
    return idx * (M_PI / 2047.0);
}

float samplePhi(std::mt19937& gen) {
    std::uniform_real_distribution<float> dis(0.0f, 2.0f * (float)M_PI);
    return dis(gen);
}
