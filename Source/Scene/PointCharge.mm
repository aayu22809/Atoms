#import "PointCharge.h"
#include <cmath>

float coulombPotential(simd_float3 point, const std::vector<PointChargeData>& charges) {
    float V = 0.0f;
    for (const auto& c : charges) {
        float r = fmax(simd_length(point - c.position), 0.01f);
        V += c.charge / r;
    }
    return V;
}

simd_float3 electricField(simd_float3 point, const std::vector<PointChargeData>& charges) {
    simd_float3 E = simd_make_float3(0, 0, 0);
    for (const auto& c : charges) {
        simd_float3 r = point - c.position;
        float dist3 = pow(simd_length(r), 3.0f);
        if (dist3 > 1e-6f) E += c.charge / dist3 * r;
    }
    return E;
}
