#include <metal_stdlib>
#include "ShaderTypes.h"
using namespace metal;

struct ChargeVertexOut {
    float4 position [[position]];
    float4 color;
    float3 localPos;
};

vertex ChargeVertexOut chargeVertex(uint vid [[vertex_id]],
                                   device const float3* positions [[buffer(0)]],
                                   constant PointCharge* charges [[buffer(3)]],
                                   constant FrameUniforms& uniforms [[buffer(1)]],
                                   uint instanceID [[instance_id]]) {
    PointCharge charge = charges[instanceID];
    float3 worldPos = positions[vid] * charge.radius + charge.position;
    ChargeVertexOut out;
    out.position = uniforms.projectionMatrix * uniforms.viewMatrix * float4(worldPos, 1.0);
    out.color = charge.color;
    out.localPos = positions[vid];
    return out;
}

fragment float4 chargeFragment(ChargeVertexOut in [[stage_in]]) {
    float3 normal = normalize(in.localPos);
    float3 lightDir = normalize(float3(1.0, 1.5, 1.0));
    float diff = max(dot(normal, lightDir), 0.2);
    float rim = 1.0 - abs(dot(normal, float3(0, 0, 1)));
    float4 glowColor = float4(in.color.rgb + float3(rim * 0.4), 1.0);
    return float4(glowColor.rgb * diff, 1.0);
}
