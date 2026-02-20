#ifndef ProbabilityCurrent_h
#define ProbabilityCurrent_h

#include <simd/simd.h>

struct Particle {
    simd_float3 pos;
    simd_float3 vel;
    simd_float4 color;
};

simd_float3 calculateProbCurrent(simd_float3 pos, int m);
void updateParticle(Particle& p, int m, float dt);

#endif
