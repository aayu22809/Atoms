#include <metal_stdlib>
using namespace metal;

struct VertexOut {
    float4 position [[position]];
    float4 color;
};

struct Uniforms {
    float4x4 view;
    float4x4 projection;
};

// buffer(0): sphere mesh vertices (float3 position, unit sphere)
// buffer(1): instance data - packed as float4 pos_radius (xyz=pos, w=radius), float4 color per instance
// buffer(2): uniforms
vertex VertexOut vertexRealtime(constant float3* vertices [[buffer(0)]],
                                constant float* instances [[buffer(1)]],
                                constant Uniforms& u [[buffer(2)]],
                                uint vid [[vertex_id]],
                                uint iid [[instance_id]]) {
    float3 spherePos = vertices[vid];
    float3 instancePos = float3(instances[iid * 8], instances[iid * 8 + 1], instances[iid * 8 + 2]);
    float radius = instances[iid * 8 + 3];
    float4 instanceColor = float4(instances[iid * 8 + 4], instances[iid * 8 + 5], instances[iid * 8 + 6], instances[iid * 8 + 7]);

    float3 worldPos = spherePos * radius + instancePos;
    float4 clipPos = u.projection * u.view * float4(worldPos, 1.0);

    VertexOut out;
    out.position = clipPos;
    out.color = instanceColor;
    return out;
}

fragment float4 fragmentRealtime(VertexOut in [[stage_in]]) {
    return in.color;
}
