#include <metal_stdlib>
#include "ShaderTypes.h"
using namespace metal;

float intersectSphere(float3 ro, float3 rd, float3 center, float radius) {
    float3 oc = ro - center;
    float a = dot(rd, rd);
    float b = 2.0 * dot(oc, rd);
    float c = dot(oc, oc) - radius * radius;
    float d = b * b - 4.0 * a * c;
    if (d < 0.0) return -1.0;
    return (-b - sqrt(d)) / (2.0 * a);
}

bool anyHit(float3 ro, float3 rd, float maxDist,
            device const Sphere* spheres, uint count) {
    for (uint i = 0; i < count; ++i) {
        float t = intersectSphere(ro, rd,
            spheres[i].centerRadius.xyz, spheres[i].centerRadius.w);
        if (t > 0.001 && t < maxDist) return true;
    }
    return false;
}

kernel void raytracerKernel(texture2d<float, access::write> outTexture [[texture(0)]],
                            device const Sphere* spheres [[buffer(2)]],
                            constant FrameUniforms& uniforms [[buffer(1)]],
                            constant uint& sphereCount [[buffer(4)]],
                            uint2 gid [[thread_position_in_grid]]) {
    uint2 size = uint2(outTexture.get_width(), outTexture.get_height());
    if (gid.x >= size.x || gid.y >= size.y) return;

    float2 uv = (float2(gid) / float2(size)) * 2.0 - 1.0;
    uv.y *= -1.0;

    float4 worldTarget = uniforms.invViewProj * float4(uv, 1.0, 1.0);
    float3 rayDir = normalize(worldTarget.xyz / worldTarget.w - uniforms.cameraPos);
    float3 rayOrigin = uniforms.cameraPos;

    float tMin = 1e20;
    int hitIdx = -1;

    for (uint i = 0; i < sphereCount; ++i) {
        float t = intersectSphere(rayOrigin, rayDir,
            spheres[i].centerRadius.xyz, spheres[i].centerRadius.w);
        if (t > 0.0 && t < tMin) {
            tMin = t;
            hitIdx = (int)i;
        }
    }

    float4 result;
    if (hitIdx >= 0) {
        float3 hitPos = rayOrigin + tMin * rayDir;
        float3 normal = normalize(hitPos - spheres[hitIdx].centerRadius.xyz);
        float3 color = spheres[hitIdx].color.rgb;
        float3 lightPos = float3(0.0, 50.0, 50.0);
        float3 lightDir = normalize(lightPos - hitPos);
        float lightDist = length(lightPos - hitPos);
        float shadow = anyHit(hitPos + normal * 0.01, lightDir, lightDist, spheres, sphereCount) ? 0.0 : 1.0;
        float diff = max(dot(normal, lightDir), 0.0);
        float3 ambient = float3(0.15) * color;
        float3 diffuse = diff * color * shadow * 2.5;
        result = float4(ambient + diffuse, 1.0);
    } else {
        result = float4(0.04, 0.04, 0.08, 1.0);
    }
    outTexture.write(result, gid);
}

struct BlitVertexOut {
    float4 position [[position]];
    float2 texCoord;
};

vertex BlitVertexOut blitVertex(uint vid [[vertex_id]],
                                device const float* vertices [[buffer(0)]]) {
    float x = vertices[vid * 2];
    float y = vertices[vid * 2 + 1];
    BlitVertexOut out;
    out.position = float4(x, y, 0, 1);
    out.texCoord = float2((x + 1) * 0.5, (1 - y) * 0.5);
    return out;
}

fragment float4 blitFragment(BlitVertexOut in [[stage_in]],
                             texture2d<float> tex [[texture(0)]]) {
    constexpr sampler s(coord::normalized, filter::linear);
    return tex.sample(s, in.texCoord);
}
