#include <metal_stdlib>
#include "ShaderTypes.h"
using namespace metal;

struct GridVertexOut {
    float4 position [[position]];
};

vertex GridVertexOut gridVertex(uint vid [[vertex_id]],
                                device const float3* positions [[buffer(0)]],
                                constant FrameUniforms& uniforms [[buffer(1)]]) {
    float4 worldPos = float4(positions[vid], 1.0);
    GridVertexOut out;
    out.position = uniforms.projectionMatrix * uniforms.viewMatrix * worldPos;
    return out;
}

fragment float4 gridFragment(GridVertexOut in [[stage_in]]) {
    return float4(0.2, 0.2, 0.2, 0.5);
}
