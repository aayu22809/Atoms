#include <metal_stdlib>
#include "ShaderTypes.h"
using namespace metal;

struct VertexOut {
    float4 position [[position]];
    float4 color;
    float  pointSize [[point_size]];
    float3 worldPos;
};

vertex VertexOut orbitalVertex(uint vertexID [[vertex_id]],
                               device const OrbitalVertex* vertices [[buffer(0)]],
                               constant FrameUniforms& uniforms [[buffer(1)]]) {
    OrbitalVertex in = vertices[vertexID];
    float4 worldPos = uniforms.modelMatrix * float4(in.position, 1.0);
    float4 viewPos = uniforms.viewMatrix * worldPos;
    float4 clipPos = uniforms.projectionMatrix * viewPos;
    VertexOut out;
    out.position = clipPos;
    out.color = in.color;
    out.worldPos = worldPos.xyz;
    out.pointSize = max(2.0, uniforms.particleRadius * 800.0 / (-viewPos.z));
    return out;
}

fragment float4 orbitalFragment(VertexOut in [[stage_in]],
                               float2 uv [[point_coord]],
                               constant FrameUniforms& uniforms [[buffer(1)]]) {
    float2 centered = uv * 2.0 - 1.0;
    float dist2 = dot(centered, centered);
    if (dist2 > 1.0) discard_fragment();
    float3 normal = normalize(float3(centered, sqrt(1.0 - dist2)));
    float3 lightDir = normalize(float3(1.0, 1.5, 1.0));
    float diff = max(dot(normal, lightDir), 0.0);
    float lighting = 0.3 + 0.7 * diff;
    return float4(in.color.rgb * lighting, in.color.a);
}
