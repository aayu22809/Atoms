#include <metal_stdlib>
using namespace metal;

struct HUDVertexOut {
    float4 position [[position]];
    float2 texCoord;
};

vertex HUDVertexOut hudVertex(uint vid [[vertex_id]],
                              device const float* vertices [[buffer(0)]]) {
    float x = vertices[vid * 4 + 0];
    float y = vertices[vid * 4 + 1];
    float u = vertices[vid * 4 + 2];
    float v = vertices[vid * 4 + 3];
    HUDVertexOut out;
    out.position = float4(x, y, 0, 1);
    out.texCoord = float2(u, v);
    return out;
}

fragment float4 hudFragment(HUDVertexOut in [[stage_in]],
                            texture2d<float> tex [[texture(0)]]) {
    constexpr sampler s(coord::normalized, filter::linear);
    float4 c = tex.sample(s, in.texCoord);
    return float4(c.rgb, c.a);
}
