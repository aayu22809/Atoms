#include <metal_stdlib>
using namespace metal;

struct VertexOut {
    float4 position [[position]];
    float2 screenPos;
};

struct RaytracerUniforms {
    float3 camera_pos;
    float _pad0;
    float4x4 inv_view_proj;
    float3 light_pos;
    float light_intensity;
    float3 ambient_light;
    float _pad1;
    int sphere_count;
};

// buffer(0): sphere_data - 2 float4s per sphere: center_radius (xyz=center, w=radius), color
// buffer(1): uniforms
vertex VertexOut vertexRaytracer(uint vid [[vertex_id]],
                                constant float2* vertices [[buffer(0)]]) {
    float2 pos = vertices[vid];
    VertexOut out;
    out.position = float4(pos.x, pos.y, 0.0, 1.0);
    out.screenPos = pos;
    return out;
}

// Ray-sphere intersection. Returns t or -1.0 for no hit.
float intersect_sphere(float3 ray_origin, float3 ray_dir, float3 sphere_center, float sphere_radius) {
    float3 oc = ray_origin - sphere_center;
    float a = dot(ray_dir, ray_dir);
    float b = 2.0 * dot(oc, ray_dir);
    float c = dot(oc, oc) - sphere_radius * sphere_radius;
    float discriminant = b * b - 4.0 * a * c;
    if (discriminant < 0.0) {
        return -1.0;
    }
    return (-b - sqrt(discriminant)) / (2.0 * a);
}

// Checks for any intersection along a ray up to max_dist (for shadows)
bool any_hit(device const float4* sphere_data, int sphere_count,
             float3 ray_origin, float3 ray_dir, float max_dist) {
    for (int i = 0; i < sphere_count; ++i) {
        float4 center_radius = sphere_data[i * 2];
        float t = intersect_sphere(ray_origin, ray_dir, center_radius.xyz, center_radius.w);
        if (t > 0.0 && t < max_dist) {
            return true;
        }
    }
    return false;
}

fragment float4 fragmentRaytracer(VertexOut in [[stage_in]],
                                  device const float4* sphere_data [[buffer(0)]],
                                  constant RaytracerUniforms& u [[buffer(1)]]) {
    // Construct ray from camera through the screen
    float4 target = u.inv_view_proj * float4(in.screenPos, 1.0, 1.0);
    float3 ray_dir = normalize(float3(target / target.w) - u.camera_pos);

    int sphere_count = u.sphere_count;

    // Find closest sphere intersection
    float t_min = 1e20;
    int closest_sphere_idx = -1;
    for (int i = 0; i < sphere_count; ++i) {
        float4 center_radius = sphere_data[i * 2];
        float t = intersect_sphere(u.camera_pos, ray_dir, center_radius.xyz, center_radius.w);
        if (t > 0.0 && t < t_min) {
            t_min = t;
            closest_sphere_idx = i;
        }
    }

    if (closest_sphere_idx != -1) {
        float4 center_radius = sphere_data[closest_sphere_idx * 2];
        float4 sphere_color_vec = sphere_data[closest_sphere_idx * 2 + 1];
        float3 hit_pos = u.camera_pos + t_min * ray_dir;
        float3 normal = normalize(hit_pos - center_radius.xyz);
        float3 sphere_color = sphere_color_vec.rgb;

        // Shadow calculation
        float3 light_dir = normalize(u.light_pos - hit_pos);
        float light_dist = length(u.light_pos - hit_pos);
        float shadow_factor = 1.0;
        if (any_hit(sphere_data, sphere_count, hit_pos + normal * 0.001, light_dir, light_dist)) {
            shadow_factor = 0.0;
        }

        // Diffuse lighting from point light
        float diff = max(dot(normal, light_dir), 0.0);
        float3 diffuse = diff * sphere_color * shadow_factor * u.light_intensity;
        float3 ambient = u.ambient_light * sphere_color * u.light_intensity;

        return float4(ambient + diffuse, sphere_color_vec.a);
    }
    return float4(0.0, 0.0, 0.0, 1.0);
}
