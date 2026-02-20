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
#include <fstream>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <complex>
#include <random>
#include <algorithm>
#include <mach-o/dyld.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

extern "C" void* attachMetalLayer(void*, void*);

const float a0 = 1;
float electron_r = 1.5f;
const double hbar = 1;
const double m_e = 1;
const double zmSpeed = 10.0;
int n = 2, l = 1, m = 0, N = 100000;

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
        {0.0f, 0.0f, 0.0f, 1.0f}, {0.5f, 0.0f, 0.99f, 1.0f}, {0.8f, 0.0f, 0.0f, 1.0f},
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
        double Lm2 = 1.0, Lm1 = 1.0 + alpha - rho;
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
    double intensity = radial * Plm * Plm;
    return heatmap_fire((float)(intensity * 1.5 * pow(5, n)));
}

struct Camera {
    glm::vec3 target = glm::vec3(0.0f);
    float radius = 50.0f;
    float azimuth = 0.0f;
    float elevation = (float)M_PI / 2.0f;
    float orbitSpeed = 0.01f;
    double zoomSpeed = zmSpeed;
    bool dragging = false;
    double lastX = 0.0, lastY = 0.0;

    glm::vec3 position() const {
        float ce = std::clamp(elevation, 0.01f, (float)M_PI - 0.01f);
        return glm::vec3(radius * sin(ce) * cos(azimuth), radius * cos(ce), radius * sin(ce) * sin(azimuth));
    }
    void processMouseMove(double x, double y) {
        float dx = (float)(x - lastX), dy = (float)(y - lastY);
        if (dragging) {
            azimuth += dx * orbitSpeed;
            elevation -= dy * orbitSpeed;
            elevation = std::clamp(elevation, 0.01f, (float)M_PI - 0.01f);
        }
        lastX = x; lastY = y;
        target = glm::vec3(0.0f);
    }
    void processMouseButton(int button, int action, GLFWwindow* win) {
        if (button == GLFW_MOUSE_BUTTON_LEFT || button == GLFW_MOUSE_BUTTON_MIDDLE) {
            dragging = (action == GLFW_PRESS);
            if (dragging) glfwGetCursorPos(win, &lastX, &lastY);
        }
    }
    void processScroll(double, double yoffset) {
        radius -= (float)yoffset * (float)zoomSpeed;
        if (radius < 1.0f) radius = 1.0f;
        target = glm::vec3(0.0f);
    }
};

glm::vec3 sphericalToCartesian(float r, float theta, float phi) {
    return glm::vec3(r * sin(theta) * cos(phi), r * cos(theta), r * sin(theta) * sin(phi));
}

void generateParticles(int Np) {
    particles.clear();
    for (int i = 0; i < Np; ++i) {
        glm::vec3 pos = sphericalToCartesian(
            (float)sampleR(n, l, gen),
            (float)sampleTheta(l, m, gen),
            samplePhi(n, l, m));
        float r = glm::length(pos);
        double theta = acos(pos.y / r);
        double phi = atan2(pos.z, pos.x);
        particles.emplace_back(pos, inferno(r, theta, phi, n, l, m));
    }
}

static std::string getShaderPath() {
    char path[1024];
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) == 0) {
        std::string exe(path);
        size_t pos = exe.find_last_of("/");
        if (pos != std::string::npos)
            return exe.substr(0, pos) + "/../shaders/realtime.metal";
    }
    return "shaders/realtime.metal";
}

static std::string loadFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) return "";
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

struct Engine {
    GLFWwindow* window = nullptr;
    int WIDTH = 800, HEIGHT = 600;
    MTL::Device* device = nullptr;
    MTL::CommandQueue* commandQueue = nullptr;
    CA::MetalLayer* metalLayer = nullptr;
    MTL::RenderPipelineState* pipeline = nullptr;
    MTL::Buffer* sphereVertexBuffer = nullptr;
    MTL::Buffer* instanceBuffer = nullptr;
    MTL::Buffer* uniformBuffer = nullptr;
    size_t sphereVertexCount = 0;
    static const size_t MAX_INSTANCES = 300000;

    Engine() {
        if (!glfwInit()) exit(-1);
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow(WIDTH, HEIGHT, "Atom Prob-Flow - Metal", nullptr, nullptr);
        if (!window) { glfwTerminate(); exit(-1); }

        device = MTL::CreateSystemDefaultDevice();
        if (!device) { glfwDestroyWindow(window); glfwTerminate(); exit(-1); }

        metalLayer = (CA::MetalLayer*)attachMetalLayer(window, device);
        if (!metalLayer) { device->release(); glfwDestroyWindow(window); glfwTerminate(); exit(-1); }

        commandQueue = device->newCommandQueue();

        std::vector<float> sphereVerts;
        float r = 1.0f;
        int stacks = 10, sectors = 10;
        for (int i = 0; i <= stacks; ++i) {
            float t1 = (float)i / stacks * (float)M_PI;
            float t2 = (float)(i + 1) / stacks * (float)M_PI;
            for (int j = 0; j < sectors; ++j) {
                float p1 = (float)j / sectors * 2.0f * (float)M_PI;
                float p2 = (float)(j + 1) / sectors * 2.0f * (float)M_PI;
                auto gp = [&](float t, float p) {
                    return glm::vec3(r * sin(t) * cos(p), r * cos(t), r * sin(t) * sin(p));
                };
                glm::vec3 v1 = gp(t1, p1), v2 = gp(t1, p2), v3 = gp(t2, p1), v4 = gp(t2, p2);
                sphereVerts.insert(sphereVerts.end(), {v1.x, v1.y, v1.z, v2.x, v2.y, v2.z, v3.x, v3.y, v3.z});
                sphereVerts.insert(sphereVerts.end(), {v2.x, v2.y, v2.z, v4.x, v4.y, v4.z, v3.x, v3.y, v3.z});
            }
        }
        sphereVertexCount = sphereVerts.size() / 3;
        sphereVertexBuffer = device->newBuffer(sphereVerts.data(), sphereVerts.size() * sizeof(float), MTL::ResourceStorageModeShared);

        NS::Error* err = nullptr;
        std::string src = loadFile(getShaderPath());
        MTL::Library* lib = src.empty() ? nullptr : device->newLibrary(NS::String::string(src.c_str(), NS::UTF8StringEncoding), nullptr, &err);
        if (lib) {
            MTL::Function* vertFn = lib->newFunction(NS::String::string("vertexRealtime", NS::UTF8StringEncoding));
            MTL::Function* fragFn = lib->newFunction(NS::String::string("fragmentRealtime", NS::UTF8StringEncoding));
            if (vertFn && fragFn) {
                MTL::RenderPipelineDescriptor* desc = MTL::RenderPipelineDescriptor::alloc()->init();
                desc->setVertexFunction(vertFn);
                desc->setFragmentFunction(fragFn);
                desc->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
                pipeline = device->newRenderPipelineState(desc, &err);
                vertFn->release();
                fragFn->release();
                desc->release();
            }
            lib->release();
        }

        instanceBuffer = device->newBuffer(MAX_INSTANCES * 8 * sizeof(float), MTL::ResourceStorageModeShared);
        uniformBuffer = device->newBuffer(128, MTL::ResourceStorageModeShared);
    }

    void drawSpheres(std::vector<Particle>& parts, Camera& camera) {
        if (!pipeline || parts.empty()) return;

        size_t count = 0;
        float* inst = (float*)instanceBuffer->contents();
        for (auto& p : parts) {
            if (p.pos.x < 0 && p.pos.y > 0) continue;
            inst[count * 8 + 0] = p.pos.x;
            inst[count * 8 + 1] = p.pos.y;
            inst[count * 8 + 2] = p.pos.z;
            inst[count * 8 + 3] = electron_r;
            inst[count * 8 + 4] = p.color.r;
            inst[count * 8 + 5] = p.color.g;
            inst[count * 8 + 6] = p.color.b;
            inst[count * 8 + 7] = p.color.a;
            count++;
        }
        if (count == 0) return;

        glm::mat4 view = glm::lookAt(camera.position(), camera.target, glm::vec3(0, 1, 0));
        glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)WIDTH / HEIGHT, 0.1f, 2000.0f);
        memcpy((char*)uniformBuffer->contents(), &view[0][0], 64);
        memcpy((char*)uniformBuffer->contents() + 64, &proj[0][0], 64);

        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        if (w <= 0 || h <= 0) return;

        metalLayer->setDrawableSize(CGSizeMake(w, h));
        CA::MetalDrawable* drawable = metalLayer->nextDrawable();
        if (!drawable) return;

        NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();

        MTL::RenderPassDescriptor* passDesc = MTL::RenderPassDescriptor::renderPassDescriptor();
        passDesc->colorAttachments()->object(0)->setTexture(drawable->texture());
        passDesc->colorAttachments()->object(0)->setLoadAction(MTL::LoadActionClear);
        passDesc->colorAttachments()->object(0)->setStoreAction(MTL::StoreActionStore);
        passDesc->colorAttachments()->object(0)->setClearColor(MTL::ClearColor::Make(0.0, 0.0, 0.0, 1.0));

        MTL::CommandBuffer* cmdBuf = commandQueue->commandBuffer();
        MTL::RenderCommandEncoder* enc = cmdBuf->renderCommandEncoder(passDesc);

        enc->setRenderPipelineState(pipeline);
        enc->setVertexBuffer(sphereVertexBuffer, 0, 0);
        enc->setVertexBuffer(instanceBuffer, 0, 1);
        enc->setVertexBuffer(uniformBuffer, 0, 2);
        enc->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(sphereVertexCount), NS::UInteger(count));

        enc->endEncoding();
        cmdBuf->presentDrawable(drawable);
        cmdBuf->commit();

        pool->release();
    }
};

static Engine* g_engine = nullptr;
static Camera camera;

int main() {
    Engine engine;
    g_engine = &engine;
    glfwSetWindowUserPointer(engine.window, &camera);
    glfwSetMouseButtonCallback(engine.window, [](GLFWwindow* win, int b, int a, int) {
        ((Camera*)glfwGetWindowUserPointer(win))->processMouseButton(b, a, win);
    });
    glfwSetCursorPosCallback(engine.window, [](GLFWwindow* win, double x, double y) {
        ((Camera*)glfwGetWindowUserPointer(win))->processMouseMove(x, y);
    });
    glfwSetScrollCallback(engine.window, [](GLFWwindow* win, double, double y) {
        ((Camera*)glfwGetWindowUserPointer(win))->processScroll(0, y);
    });
    glfwSetKeyCallback(engine.window, [](GLFWwindow*, int key, int, int action, int) {
        if (action != GLFW_PRESS && action != GLFW_REPEAT) return;
        if (key == GLFW_KEY_W) { n++; generateParticles(N); }
        else if (key == GLFW_KEY_S) { n = std::max(1, n - 1); generateParticles(N); }
        else if (key == GLFW_KEY_E) { l++; generateParticles(N); }
        else if (key == GLFW_KEY_D) { l = std::max(0, l - 1); generateParticles(N); }
        else if (key == GLFW_KEY_R) { m++; generateParticles(N); }
        else if (key == GLFW_KEY_F) { m--; generateParticles(N); }
        else if (key == GLFW_KEY_T) { N += 100000; generateParticles(N); }
        else if (key == GLFW_KEY_G) { N = std::max(1000, N - 100000); generateParticles(N); }
        if (l > n - 1) l = n - 1;
        if (l < 0) l = 0;
        if (m > l) m = l;
        if (m < -l) m = -l;
        electron_r = (float)n / 3.0f;
        std::cout << "n=" << n << " l=" << l << " m=" << m << " N=" << N << "\n";
    });

    electron_r = (float)n / 3.0f;
    generateParticles(std::min(N, 250000));

    float dt = 0.5f;
    while (!glfwWindowShouldClose(engine.window)) {
        for (Particle& p : particles) {
            double r = glm::length(p.pos);
            if (r > 1e-6) {
                double theta = acos(p.pos.y / r);
                p.vel = calculateProbabilityFlow(p, n, l, m);
                glm::vec3 temp = p.pos + p.vel * dt;
                double new_phi = atan2(temp.z, temp.x);
                p.pos = sphericalToCartesian((float)r, (float)theta, (float)new_phi);
            }
        }
        engine.drawSpheres(particles, camera);
        glfwPollEvents();
    }

    engine.sphereVertexBuffer->release();
    engine.instanceBuffer->release();
    engine.uniformBuffer->release();
    if (engine.pipeline) engine.pipeline->release();
    engine.commandQueue->release();
    engine.device->release();
    glfwDestroyWindow(engine.window);
    glfwTerminate();
    return 0;
}
