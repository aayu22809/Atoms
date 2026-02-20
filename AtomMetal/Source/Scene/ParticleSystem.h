#import <Foundation/Foundation.h>
#include "ProbabilityCurrent.h"
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <random>

struct CDFCache;

void generateParticles(int n, int l, int m, int count,
                      std::vector<Particle>& out,
                      std::unordered_map<uint64_t, struct CDFCache>& cacheMap,
                      std::mt19937& gen,
                      int colorMode = 0);
void updateParticles(std::vector<Particle>& particles, int m, float dt);
