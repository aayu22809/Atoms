#ifndef CDFSampler_h
#define CDFSampler_h

#include <random>
#include <unordered_map>
#include <vector>
#include <cstdint>

struct CDFCache {
    std::vector<double> radialCDF;
    std::vector<double> thetaCDF;
    int n = 0, l = 0, m = 0;
    bool built = false;
};

double sampleR(int n, int l, std::mt19937& gen, std::unordered_map<uint64_t, CDFCache>& cacheMap);
double sampleTheta(int l, int m, std::mt19937& gen, std::unordered_map<uint64_t, CDFCache>& cacheMap);
float samplePhi(std::mt19937& gen);

#endif
