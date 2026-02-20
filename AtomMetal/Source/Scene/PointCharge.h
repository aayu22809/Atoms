#import <Foundation/Foundation.h>
#include <simd/simd.h>
#include <vector>

struct PointChargeData {
    simd_float3 position;
    float charge;
    simd_float4 color;
    float radius;
};

float coulombPotential(simd_float3 point, const std::vector<PointChargeData>& charges);
simd_float3 electricField(simd_float3 point, const std::vector<PointChargeData>& charges);
