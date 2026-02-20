#define NS_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#include <Foundation/Foundation.hpp>
#include <QuartzCore/QuartzCore.hpp>
#include <Metal/Metal.hpp>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <iostream>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <chrono>
#include <fstream>
#include <complex>
#include <random>
#include <algorithm>
#include <mach-o/dyld.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

extern "C" void* attachMetalLayer(void*, void*);

// ================= Constants ================= //
const float a0 = 1;
const float electron_r = 0.25f;
const double hbar = 1;
const double m_e = 1;
const double zmSpeed = 10.0;

int N = 100000;
float LightingScaler = 700;
float n = 3, l = 1, m = 1;

// ================= Physics Sampling ================= //
struct Particle {
    glm::vec3 pos;
    glm::vec3 vel = glm::vec3(0.0f);
    glm::vec4 color;
    Particle(glm::vec3 p, glm::vec4 c = glm::vec4(0.0f, 0.5f, 1.0f, 1.0f)) : pos(p), color(c) {}
};
std::vector<Particle> particles;

std::random_device rd;
std::mt19937 gen(rd());
std::uniform_real_distribution<float> dis(0.0f, 1.0f);

double sampleR(int n, int l, std::mt19937& gen) {
    const int Nc = 4096;
    const double rMax = 10.0 * n * n * a0;
    static std::vector<double> cdf;
    static bool built = false;
    if (!built) {
        cdf.resize(Nc);
        double dr = rMax / (Nc - 1);
        double sum = 0.0;
        for (int i = 0; i < Nc; ++i) {
            double r = i * dr;
            double rho = 2.0 * r / (n * a0);
            int k = n - l - 1;
            int alpha = 2 * l + 1;
            double L = 1.0, Lm1 = 1.0 + alpha - rho;
            if (k == 1) L = Lm1;
            else if (k > 1) {
                double Lm2 = 1.0;
                for (int j = 2; j <= k; ++j) {
                    L = ((2*j - 1 + alpha - rho) * Lm1 - (j - 1 + alpha) * Lm2) / j;
                    Lm2 = Lm1; Lm1 = L;
                }
            }
            double norm = pow(2.0 / (n * a0), 3) * tgamma(n - l) / (2.0 * n * tgamma(n + l + 1));
            double R = sqrt(norm) * exp(-rho / 2.0) * pow(rho, l) * L;
            sum += r * r * R * R;
            cdf[i] = sum;
        }
        for (double& v : cdf) v /= sum;
        built = true;
    }
    std::uniform_real_distribution<double> udis(0.0, 1.0);
    double u = udis(gen);
    int idx = std::lower_bound(cdf.begin(), cdf.end(), u) - cdf.begin();
    return idx * (rMax / (Nc - 1));
}

double sampleTheta(int l, int m, std::mt19937& gen) {
    const int Nc = 2048;
    static std::vector<double> cdf;
    static bool built = false;
    if (!built) {
        cdf.resize(Nc);
        double dtheta = M_PI / (Nc - 1);
        double sum = 0.0;
        for (int i = 0; i < Nc; ++i) {
            double theta = i * dtheta;
            double x = cos(theta);
            double Pmm = 1.0;
            if (m > 0) {
                double somx2 = sqrt((1.0 - x) * (1.0 + x));
                double fact = 1.0;
                for (int j = 1; j <= m; ++j) { Pmm *= -fact * somx2; fact += 2.0; }
            }
            double Plm;
            if (l == m) Plm = Pmm;
            else {
                double Pm1m = x * (2 * m + 1) * Pmm;
                if (l == m + 1) Plm = Pm1m;
                else {
                    for (int ll = m + 2; ll <= l; ++ll) {
                        double Pll = ((2*ll - 1) * x * Pm1m - (ll + m - 1) * Pmm) / (ll - m);
                        Pmm = Pm1m; Pm1m = Pll;
                    }
                    Plm = Pm1m;
                }
            }
            sum += sin(theta) * Plm * Plm;
            cdf[i] = sum;
        }
        for (double& v : cdf) v /= sum;
        built = true;
    }
    std::uniform_real_distribution<double> udis(0.0, 1.0);
    int idx = std::lower_bound(cdf.begin(), cdf.end(), udis(gen)) - cdf.begin();
    return idx * (M_PI / (Nc - 1));
}

float samplePhi(float, float, float) { return 2.0f * (float)M_PI * dis(gen); }

glm::vec3 calculateProbabilityFlow(Particle& p, int n, int l, int m) {
    double r = glm::length(p.pos);
    if (r < 1e-6) return glm::vec3(0.0f);
    double theta = acos(p.pos.y / r);
    double phi = atan2(p.pos.z, p.pos.x);
    double sinTheta = sin(theta);
    if (fabs(sinTheta) < 1e-4) sinTheta = 1e-4;
    double v_mag = hbar * m / (m_e * r * sinTheta);
    return glm::vec3(-(float)(v_mag * sin(phi)), 0.0f, (float)(v_mag * cos(phi)));
}

glm::vec4 heatmap_fire(float value) {
    value = std::max(0.0f, std::min(1.0f, value));
    const int num_stops = 6;
    glm::vec4 colors[6] = {
        {0.0f, 0.0f, 0.0f, 1.0f}, {0.3f, 0.0f, 0.6f, 1.0f}, {0.8f, 0.0f, 0.0f, 1.0f},
        {1.0f, 0.5f, 0.0f, 1.0f}, {1.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}
    };
    float scaled_v = value * (num_stops - 1);
    int i = (int)scaled_v;
    int next_i = std::min(i + 1, num_stops - 1);
    float local_t = scaled_v - i;
    return glm::vec4(
        colors[i].r + local_t * (colors[next_i].r - colors[i].r),
        colors[i].g + local_t * (colors[next_i].g - colors[i].g),
        colors[i].b + local_t * (colors[next_i].b - colors[i].b),
        1.0f);
}

glm::vec4 inferno(double r, double theta, double phi, int n, int l, int m) {
    double rho = 2.0 * r / (n * a0);
    int k = n - l - 1;
    int alpha = 2 * l + 1;
    double L = 1.0;
    if (k == 1) L = 1.0 + alpha - rho;
    else if (k > 1) {
        double Lm2 = 1.0;
        double Lm1 = 1.0 + alpha - rho;
        for (int j = 2; j <= k; ++j) {
            L = ((2*j - 1 + alpha - rho) * Lm1 - (j - 1 + alpha) * Lm2) / j;
            Lm2 = Lm1; Lm1 = L;
        }
    }
    double norm = pow(2.0 / (n * a0), 3) * tgamma(n - l) / (2.0 * n * tgamma(n + l + 1));
    double R = sqrt(norm) * exp(-rho / 2.0) * pow(rho, l) * L;
    double radial = R * R;

    double x = cos(theta);
    double Pmm = 1.0;
    if (m > 0) {
        double somx2 = sqrt((1.0 - x) * (1.0 + x));
        double fact = 1.0;
        for (int j = 1; j <= m; ++j) { Pmm *= -fact * somx2; fact += 2.0; }
    }
    double Plm;
    if (l == m) Plm = Pmm;
    else {
        double Pm1m = x * (2*m + 1) * Pmm;
        if (l == m + 1) Plm = Pm1m;
        else {
            for (int ll = m + 2; ll <= l; ++ll) {
                double Pll = ((2*ll - 1) * x * Pm1m - (ll + m - 1) * Pmm) / (ll - m);
                Pmm = Pm1m; Pm1m = Pll;
            }
            Plm = Pm1m;
        }
    }
    double angular = Plm * Plm;
    double intensity = radial * angular;
    return heatmap_fire((float)(intensity * LightingScaler));
}

// ================= Raytracer ================= //
struct Sphere { glm::vec4 center_radius; glm::vec4 color; };

struct Camera {
    glm::vec3 target = glm::vec3(0.0f, 0.0f, 0.0f);
    float radius = 50.0f;
    float azimuth = 0.0f;
    float elevation = (float)(M_PI / 2.0);
    float orbitSpeed = 0.01f;
    float panSpeed = 0.01f;
    double zoomSpeed = zmSpeed;
    bool dragging = false;
    bool panning = false;
    double lastX = 0.0, lastY = 0.0;

    glm::vec3 position() const {
        float clampedElevation = std::clamp(elevation, 0.01f, (float)M_PI - 0.01f);
        return glm::vec3(
            radius * sin(clampedElevation) * cos(azimuth),
            radius * cos(clampedElevation),
            radius * sin(clampedElevation) * sin(azimuth));
    }
    void update() { target = glm::vec3(0.0f, 0.0f, 0.0f); }

    void processMouseMove(double x, double y) {
        float dx = (float)(x - lastX);
        float dy = (float)(y - lastY);
        if (dragging) {
            azimuth += dx * orbitSpeed;
            elevation -= dy * orbitSpeed;
            elevation = std::clamp(elevation, 0.01f, (float)M_PI - 0.01f);
        }
        lastX = x; lastY = y;
        update();
    }
    void processMouseButton(int button, int action, int mods, GLFWwindow* win) {
        if (button == GLFW_MOUSE_BUTTON_LEFT || button == GLFW_MOUSE_BUTTON_MIDDLE) {
            if (action == GLFW_PRESS) {
                dragging = true;
                glfwGetCursorPos(win, &lastX, &lastY);
            } else if (action == GLFW_RELEASE) {
                dragging = false;
            }
        }
    }
    void processScroll(double xoffset, double yoffset) {
        radius -= (float)yoffset * (float)zoomSpeed;
        if (radius < 1.0f) radius = 1.0f;
        update();
    }
};
Camera camera;

glm::vec3 sphericalToCartesian(float r, float theta, float phi) {
    return glm::vec3(r * sin(theta) * cos(phi), r * cos(theta), r * sin(theta) * sin(phi));
}

void generateParticles(int count) {
    particles.clear();
    for (int i = 0; i < count; ++i) {
        glm::vec3 pos = sphericalToCartesian(
            (float)sampleR((int)n, (int)l, gen),
            (float)sampleTheta((int)l, (int)m, gen),
            samplePhi(n, l, m));
        float r = glm::length(pos);
        double theta = acos(pos.y / r);
        double phi = atan2(pos.z, pos.x);
        glm::vec4 col = inferno(r, theta, phi, (int)n, (int)l, (int)m);
        particles.emplace_back(pos, col);
    }
}

static std::string getShaderPath(const std::string& name) {
    char path[1024];
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) == 0) {
        std::string exe(path);
        size_t pos = exe.find_last_of("/");
        if (pos != std::string::npos) {
            return exe.substr(0, pos) + "/../shaders/" + name + ".metal";
        }
    }
    return "shaders/" + name + ".metal";
}

static std::string loadFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) return "";
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

struct RaytracerUniforms {
    glm::vec3 camera_pos;
    float _pad0;
    glm::mat4 inv_view_proj;
    glm::vec3 light_pos;
    float light_intensity;
    glm::vec3 ambient_light;
    float _pad1;
    int sphere_count;
};

struct Engine {
    GLFWwindow* window = nullptr;
    int WIDTH = 800;
    int HEIGHT = 600;

    MTL::Device* device = nullptr;
    MTL::CommandQueue* commandQueue = nullptr;
    CA::MetalLayer* metalLayer = nullptr;
    MTL::RenderPipelineState* pipeline = nullptr;
    MTL::Buffer* quadVertexBuffer = nullptr;
    MTL::Buffer* sphereBuffer = nullptr;
    MTL::Buffer* uniformBuffer = nullptr;
    static const int MAX_SPHERES = 200000;

    Engine() {
        if (!glfwInit()) { std::cerr << "GLFW init failed\n"; exit(EXIT_FAILURE); }
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow(WIDTH, HEIGHT, "Quantum Simulation - Raytraced (Metal)", nullptr, nullptr);
        if (!window) { glfwTerminate(); exit(EXIT_FAILURE); }

        device = MTL::CreateSystemDefaultDevice();
        if (!device) { glfwDestroyWindow(window); glfwTerminate(); exit(EXIT_FAILURE); }

        metalLayer = (CA::MetalLayer*)attachMetalLayer(window, device);
        if (!metalLayer) { device->release(); glfwDestroyWindow(window); glfwTerminate(); exit(EXIT_FAILURE); }

        commandQueue = device->newCommandQueue();

        // Fullscreen quad
        float quadVertices[] = {
            -1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f
        };
        quadVertexBuffer = device->newBuffer(quadVertices, sizeof(quadVertices), MTL::ResourceStorageModeShared);

        sphereBuffer = device->newBuffer(MAX_SPHERES * 2 * sizeof(glm::vec4), MTL::ResourceStorageModeShared);
        uniformBuffer = device->newBuffer(sizeof(RaytracerUniforms), MTL::ResourceStorageModeShared);

        std::string shaderPath = getShaderPath("raytracer");
        std::string source = loadFile(shaderPath);
        if (source.empty()) {
            std::cerr << "Failed to load raytracer.metal from " << shaderPath << "\n";
            return;
        }

        NS::Error* err = nullptr;
        NS::String* srcStr = NS::String::string(source.c_str(), NS::UTF8StringEncoding);
        MTL::CompileOptions* opts = MTL::CompileOptions::alloc()->init();
        MTL::Library* lib = device->newLibrary(srcStr, opts, &err);
        opts->release();
        if (!lib) {
            std::cerr << "Metal shader compile error: " << (err ? err->localizedDescription()->utf8String() : "unknown") << "\n";
            return;
        }

        MTL::Function* vertFn = lib->newFunction(NS::String::string("vertexRaytracer", NS::UTF8StringEncoding));
        MTL::Function* fragFn = lib->newFunction(NS::String::string("fragmentRaytracer", NS::UTF8StringEncoding));
        lib->release();
        if (!vertFn || !fragFn) {
            std::cerr << "Failed to get raytracer shader functions\n";
            if (vertFn) vertFn->release();
            if (fragFn) fragFn->release();
            return;
        }

        MTL::RenderPipelineDescriptor* desc = MTL::RenderPipelineDescriptor::alloc()->init();
        desc->setVertexFunction(vertFn);
        desc->setFragmentFunction(fragFn);
        desc->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
        pipeline = device->newRenderPipelineState(desc, &err);
        vertFn->release();
        fragFn->release();
        desc->release();
        if (!pipeline) {
            std::cerr << "Pipeline creation failed: " << (err ? err->localizedDescription()->utf8String() : "unknown") << "\n";
            return;
        }
    }

    void runRayTracer(const std::vector<Sphere>& sphere_data) {
        if (sphere_data.empty()) return;
        int count = (int)sphere_data.size();

        memcpy(sphereBuffer->contents(), sphere_data.data(), count * sizeof(Sphere));

        glm::mat4 view = glm::lookAt(camera.position(), camera.target, glm::vec3(0, 1, 0));
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)WIDTH / HEIGHT, 0.1f, 10000.0f);
        glm::mat4 invViewProj = glm::inverse(projection * view);

        RaytracerUniforms u = {};
        u.camera_pos = camera.position();
        u.inv_view_proj = invViewProj;
        u.light_pos = glm::vec3(0.0f, 50.0f, 50.0f);
        u.ambient_light = glm::vec3(0.2f);
        u.light_intensity = 3.0f;
        u.sphere_count = count;
        memcpy(uniformBuffer->contents(), &u, sizeof(u));

        CA::MetalDrawable* drawable = metalLayer->nextDrawable();
        if (!drawable) return;

        MTL::RenderPassDescriptor* passDesc = MTL::RenderPassDescriptor::renderPassDescriptor();
        passDesc->colorAttachments()->object(0)->setTexture(drawable->texture());
        passDesc->colorAttachments()->object(0)->setLoadAction(MTL::LoadActionClear);
        passDesc->colorAttachments()->object(0)->setStoreAction(MTL::StoreActionStore);
        passDesc->colorAttachments()->object(0)->setClearColor(MTL::ClearColor::Make(0.0, 0.0, 0.0, 1.0));

        MTL::CommandBuffer* cmdBuf = commandQueue->commandBuffer();
        MTL::RenderCommandEncoder* enc = cmdBuf->renderCommandEncoder(passDesc);

        enc->setRenderPipelineState(pipeline);
        enc->setVertexBuffer(quadVertexBuffer, 0, 0);
        enc->setFragmentBuffer(sphereBuffer, 0, 0);
        enc->setFragmentBuffer(uniformBuffer, 0, 1);
        enc->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(6));

        enc->endEncoding();
        cmdBuf->presentDrawable(drawable);
        cmdBuf->commit();
    }

    void setupCameraCallbacks() {
        glfwSetWindowUserPointer(window, &camera);
        glfwSetMouseButtonCallback(window, [](GLFWwindow* win, int button, int action, int mods) {
            ((Camera*)glfwGetWindowUserPointer(win))->processMouseButton(button, action, mods, win);
        });
        glfwSetCursorPosCallback(window, [](GLFWwindow* win, double x, double y) {
            ((Camera*)glfwGetWindowUserPointer(win))->processMouseMove(x, y);
        });
        glfwSetScrollCallback(window, [](GLFWwindow* win, double xoffset, double yoffset) {
            ((Camera*)glfwGetWindowUserPointer(win))->processScroll(xoffset, yoffset);
        });
        glfwSetKeyCallback(window, [](GLFWwindow* win, int key, int scancode, int action, int mods) {
            if (!(action == GLFW_PRESS || action == GLFW_REPEAT)) return;
            Camera* cam = (Camera*)glfwGetWindowUserPointer(win);
            if (key == GLFW_KEY_W) { n += 1; generateParticles(N); }
            else if (key == GLFW_KEY_S) { n = std::max(1.0f, n - 1.0f); generateParticles(N); }
            else if (key == GLFW_KEY_E) { l += 1; generateParticles(N); }
            else if (key == GLFW_KEY_D) { l = std::max(0.0f, l - 1.0f); generateParticles(N); }
            else if (key == GLFW_KEY_R) { m += 1; generateParticles(N); }
            else if (key == GLFW_KEY_F) { m -= 1; generateParticles(N); }
            else if (key == GLFW_KEY_T) { N *= 10; generateParticles(N); }
            else if (key == GLFW_KEY_G) { N = std::max(1, N / 10); generateParticles(N); }
            (void)cam;
            l = std::clamp(l, 0.0f, n - 1.0f);
            m = std::clamp(m, -l, l);
            std::cout << "Quantum numbers: n=" << n << " l=" << l << " m=" << m << " N=" << N << "\n";
        });
    }
};
Engine engine;

int main() {
    engine.setupCameraCallbacks();

    generateParticles(N);

    float dt = 0.5f;
    std::cout << "Starting raytraced simulation...\n";

    while (!glfwWindowShouldClose(engine.window)) {
        std::vector<Sphere> sphere_data;
        for (Particle& p : particles) {
            double r = glm::length(p.pos);
            if (r > 1e-6) {
                double theta = acos(p.pos.y / r);
                p.vel = calculateProbabilityFlow(p, (int)n, (int)l, (int)m);
                glm::vec3 temp_pos = p.pos + p.vel * dt;
                double new_phi = atan2(temp_pos.z, temp_pos.x);
                p.pos = sphericalToCartesian((float)r, (float)theta, (float)new_phi);
            }
            if (p.pos.z < 0 || p.pos.y < 0)
                sphere_data.push_back({glm::vec4(p.pos, electron_r), p.color});
        }
        engine.runRayTracer(sphere_data);

        glfwPollEvents();
    }

    if (engine.sphereBuffer) engine.sphereBuffer->release();
    if (engine.quadVertexBuffer) engine.quadVertexBuffer->release();
    if (engine.uniformBuffer) engine.uniformBuffer->release();
    if (engine.pipeline) engine.pipeline->release();
    if (engine.commandQueue) engine.commandQueue->release();
    if (engine.metalLayer) engine.metalLayer->release();
    if (engine.device) engine.device->release();
    glfwDestroyWindow(engine.window);
    glfwTerminate();
    return 0;
}
