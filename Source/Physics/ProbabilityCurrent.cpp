#include "ProbabilityCurrent.h"
#include <cmath>

simd_float3 calculateProbCurrent(simd_float3 pos, int m) {
    float r = simd_length(pos);
    if (r < 1e-6f) return simd_make_float3(0, 0, 0);
    float theta = acos(pos.y / r);
    float phi = atan2(pos.z, pos.x);
    float sinTheta = fmax(sin(theta), 1e-4f);
    float v_mag = (float)m / (r * sinTheta);
    return simd_make_float3(-v_mag * sin(phi), 0.0f, v_mag * cos(phi));
}

void updateParticle(Particle& p, int m, float dt) {
    float r = simd_length(p.pos);
    if (r < 1e-6f) return;
    float theta = acos(p.pos.y / r);
    simd_float3 vel = calculateProbCurrent(p.pos, m);
    simd_float3 tmp = p.pos + vel * dt;
    float newPhi = atan2(tmp.z, tmp.x);
    p.pos = simd_make_float3(
        r * sin(theta) * cos(newPhi),
        r * cos(theta),
        r * sin(theta) * sin(newPhi)
    );
}
