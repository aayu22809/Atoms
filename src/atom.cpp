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
#include <mach-o/dyld.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

extern "C" void* attachMetalLayer(void*, void*);

float orbitDistance = 15.0f;

static std::string getShaderSourcePath() {
    char path[1024];
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) == 0) {
        std::string exe(path);
        size_t pos = exe.find_last_of("/");
        if (pos != std::string::npos)
            return exe.substr(0, pos) + "/../shaders/quad_2d.metal";
    }
    return "shaders/quad_2d.metal";
}

static std::string loadFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) return "";
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

struct WavePoint { glm::vec2 localPos; glm::vec2 dir; };

struct Wave {
    float energy;
    float sigma = 40.0f, k = 0.4f, phase = 0.0f, a = 10.0f;
    std::vector<WavePoint> points;
    glm::vec2 pos, dir;
    glm::vec3 col;

    Wave(float e, glm::vec2 pos, glm::vec2 dir, glm::vec3 col = glm::vec3(0.0f, 1.0f, 1.0f))
        : energy(e), pos(pos), dir(dir), col(col) {
        dir = glm::normalize(dir);
        for (float x = -sigma; x <= sigma; x += 0.1f)
            points.push_back({pos + x * dir, dir * 200.0f});
    }

    bool update(float dt, int w, int h) {
        phase += 30.0f * dt;
        float hw = w / 2.0f, hh = h / 2.0f;
        for (WavePoint& p : points) {
            p.localPos += p.dir * dt;
            if (p.localPos.x < -hw || p.localPos.x > hw || p.localPos.y < -hh || p.localPos.y > hh)
                return true;
        }
        return false;
    }
};

struct Particle {
    glm::vec2 pos;
    int charge;
    float angle = 0.0f;
    int n = 1;
    float excitedTimer = 0.0f;
    Particle(glm::vec2 pos, int charge) : pos(pos), charge(charge) {}

    void update(glm::vec2 c, std::vector<Wave>& waves) {
        float r = n * orbitDistance;
        angle += 0.05f;
        pos = glm::vec2(cos(angle) * r + c.x, sin(angle) * r + c.y);

        if (excitedTimer <= 0.0f && n > 1) {
            n--;
            excitedTimer += 0.003f;
            float waveDirX = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
            float waveDirY = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
            float energyDiff = -13.6f / ((n + 1) * (n + 1)) - (-13.6f / (n * n));
            waves.emplace_back(energyDiff, pos, glm::vec2(waveDirX, waveDirY), glm::vec3(1.0f, 1.0f, 0.0f));
        }
    }
};

struct Atom {
    glm::vec2 pos;
    glm::vec2 v = glm::vec2(0.0f);
    std::vector<Particle> particles;
    Atom(glm::vec2 p) : pos(p) {
        particles.emplace_back(pos, 1);
        particles.emplace_back(glm::vec2(pos.x - orbitDistance, pos.y), -1);
    }
};

struct Engine {
    GLFWwindow* window = nullptr;
    int WIDTH = 800, HEIGHT = 600;
    MTL::Device* device = nullptr;
    MTL::CommandQueue* commandQueue = nullptr;
    CA::MetalLayer* metalLayer = nullptr;
    MTL::RenderPipelineState* pipeline = nullptr;
    MTL::Buffer* vertexBuffer = nullptr;
    MTL::Buffer* uniformBuffer = nullptr;
    static const size_t MAX_VERTICES = 500000;

    Engine() {
        if (!glfwInit()) { std::cerr << "GLFW init failed\n"; exit(EXIT_FAILURE); }
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow(WIDTH, HEIGHT, "2D Atom Sim - Metal", nullptr, nullptr);
        if (!window) { glfwTerminate(); exit(EXIT_FAILURE); }

        device = MTL::CreateSystemDefaultDevice();
        if (!device) { glfwDestroyWindow(window); glfwTerminate(); exit(EXIT_FAILURE); }

        metalLayer = (CA::MetalLayer*)attachMetalLayer(window, device);
        if (!metalLayer) { device->release(); glfwDestroyWindow(window); glfwTerminate(); exit(EXIT_FAILURE); }

        commandQueue = device->newCommandQueue();

        NS::Error* err = nullptr;
        std::string src = loadFile(getShaderSourcePath());
        MTL::Library* lib = src.empty() ? nullptr : device->newLibrary(NS::String::string(src.c_str(), NS::UTF8StringEncoding), nullptr, &err);
        if (lib) {
            MTL::Function* vertFn = lib->newFunction(NS::String::string("vertex2d", NS::UTF8StringEncoding));
            MTL::Function* fragFn = lib->newFunction(NS::String::string("fragment2d", NS::UTF8StringEncoding));
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

        vertexBuffer = device->newBuffer(MAX_VERTICES * 6 * sizeof(float), MTL::ResourceStorageModeShared);
        uniformBuffer = device->newBuffer(64, MTL::ResourceStorageModeShared);
    }

    void buildVertices(std::vector<float>& vertices, size_t& lineCount, size_t& triCount,
                       std::vector<Atom>& atoms, std::vector<Wave>& waves) {
        vertices.clear();
        lineCount = 0;
        triCount = 0;

        for (Wave& w : waves) {
            if (w.energy == 0.0f) continue;
            glm::vec2 perpBase(-w.points[0].dir.y, w.points[0].dir.x);
            if (glm::length(perpBase) > 1e-6f) perpBase = glm::normalize(perpBase);
            for (WavePoint& p : w.points) {
                glm::vec2 perp(-p.dir.y, p.dir.x);
                if (glm::length(perp) > 1e-6f) perp = glm::normalize(perp);
                float y_disp = w.a * sin(w.k * glm::length(p.localPos) - w.phase);
                glm::vec2 drawPos = p.localPos + perp * y_disp;
                vertices.insert(vertices.end(), {drawPos.x, drawPos.y, w.col.r, w.col.g, w.col.b, 1.0f});
            }
            lineCount += w.points.size();
        }

        for (Atom& a : atoms) {
            for (Particle& p : a.particles) {
                if (p.charge == -1) {
                    int segs = 50;
                    for (int i = 0; i <= segs; i++) {
                        float ang = 2.0f * (float)M_PI * i / segs;
                        float x = cos(ang) * p.n * orbitDistance + a.pos.x;
                        float y = sin(ang) * p.n * orbitDistance + a.pos.y;
                        vertices.insert(vertices.end(), {x, y, 0.4f, 0.4f, 0.4f, 1.0f});
                    }
                    lineCount += segs + 1;
                }
                float rad = (p.charge == -1) ? 2.0f : 5.0f;
                float cr = (p.charge == -1) ? 0.0f : 1.0f, cg = (p.charge == -1) ? 1.0f : 0.0f, cb = (p.charge == -1) ? 1.0f : 0.0f;
                int segs = 50;
                for (int i = 0; i < segs; i++) {
                    float a0 = 2.0f * (float)M_PI * i / segs;
                    float a1 = 2.0f * (float)M_PI * (i + 1) / segs;
                    vertices.insert(vertices.end(), {p.pos.x, p.pos.y, cr, cg, cb, 1.0f});
                    vertices.insert(vertices.end(), {cos(a0) * rad + p.pos.x, sin(a0) * rad + p.pos.y, cr, cg, cb, 1.0f});
                    vertices.insert(vertices.end(), {cos(a1) * rad + p.pos.x, sin(a1) * rad + p.pos.y, cr, cg, cb, 1.0f});
                }
                triCount += segs * 3;
            }
        }
    }

    void render(std::vector<Atom>& atoms, std::vector<Wave>& waves) {
        if (!pipeline) return;

        std::vector<float> vertices;
        size_t lineCount, triCount;
        buildVertices(vertices, lineCount, triCount, atoms, waves);
        if (vertices.empty()) return;

        memcpy(vertexBuffer->contents(), vertices.data(), vertices.size() * sizeof(float));

        float halfW = WIDTH / 2.0f, halfH = HEIGHT / 2.0f;
        glm::mat4 ortho = glm::ortho(-halfW, halfW, -halfH, halfH, -1.0f, 1.0f);
        memcpy(uniformBuffer->contents(), &ortho[0][0], 64);

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
        enc->setVertexBuffer(vertexBuffer, 0, 0);
        enc->setVertexBuffer(uniformBuffer, 0, 1);

        if (lineCount > 0)
            enc->drawPrimitives(MTL::PrimitiveTypeLine, NS::UInteger(0), NS::UInteger(lineCount));
        if (triCount > 0)
            enc->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(lineCount), NS::UInteger(triCount));

        enc->endEncoding();
        cmdBuf->presentDrawable(drawable);
        cmdBuf->commit();

        pool->release();
    }
};

static Engine* g_engine = nullptr;
static std::vector<Atom> atoms;
static std::vector<Wave> waves;

static void mouseButtonCallback(GLFWwindow* window, int button, int action, int) {
    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS) return;
    Engine* eng = (Engine*)glfwGetWindowUserPointer(window);
    double mx, my;
    glfwGetCursorPos(window, &mx, &my);
    float worldX = (float)mx - eng->WIDTH / 2.0f;
    float worldY = eng->HEIGHT / 2.0f - (float)my;
    glm::vec2 spawnPos(worldX, worldY);
    float energyN1toN2 = -13.6f / (2 * 2) - (-13.6f);
    for (int i = 0; i < 25; i++) {
        float angle = ((float)rand() / RAND_MAX) * 2.0f * (float)M_PI;
        waves.emplace_back(energyN1toN2, spawnPos, glm::vec2(cos(angle), sin(angle)));
    }
}

int main() {
    Engine engine;
    g_engine = &engine;
    glfwSetWindowUserPointer(engine.window, &engine);
    glfwSetMouseButtonCallback(engine.window, mouseButtonCallback);

    int num_atoms = 20;
    float radius = 100.0f;
    for (int i = 0; i < num_atoms; i++) {
        float angle = 2.0f * (float)M_PI * i / num_atoms;
        atoms.emplace_back(glm::vec2(cos(angle) * radius, sin(angle) * radius));
    }

    float energyN1toN2 = -13.6f / (2 * 2) - (-13.6f);
    for (int i = 0; i < 24; i++)
        waves.emplace_back(energyN1toN2, glm::vec2(200.0f, i * 20.0f - 200.0f), glm::vec2(-1.0f, 0.0f));

    while (!glfwWindowShouldClose(engine.window)) {
        for (Atom& a : atoms) {
            for (Atom& a2 : atoms) {
                if (&a2 == &a) continue;
                float dist = glm::length(a.pos - a2.pos);
                if (dist < 1e-6f) continue;
                glm::vec2 dir = glm::normalize(a.pos - a2.pos);
                a.v += dir / dist * 57.5f;
            }
            const float boundary_stiffness = 0.01f;
            const float boundary_threshold = 200.0f;
            float hw = engine.WIDTH / 2.0f, hh = engine.HEIGHT / 2.0f;
            float dist_left = a.pos.x + hw;
            if (dist_left < boundary_threshold) a.v.x += (boundary_threshold - dist_left) * boundary_stiffness;
            float dist_right = hw - a.pos.x;
            if (dist_right < boundary_threshold) a.v.x -= (boundary_threshold - dist_right) * boundary_stiffness;
            float dist_top = hh - a.pos.y;
            if (dist_top < boundary_threshold) a.v.y -= (boundary_threshold - dist_top) * boundary_stiffness;
            float dist_bottom = a.pos.y + hh;
            if (dist_bottom < boundary_threshold) a.v.y += (boundary_threshold - dist_bottom) * boundary_stiffness;
            a.v *= 0.99f;

            for (Particle& p : a.particles) {
                if (p.charge == 1) p.pos = a.pos;
                if (p.charge == -1) {
                    if (p.excitedTimer > 0.0f) p.excitedTimer -= 0.001f;
                    p.update(a.pos, waves);
                    for (Wave& wave : waves) {
                        if (wave.energy == 0.0f || wave.col == glm::vec3(1.0f, 1.0f, 0.0f)) continue;
                        float energyforUp = -13.6f / ((p.n + 1) * (p.n + 1)) - (-13.6f / (p.n * p.n));
                        if (wave.energy != energyforUp) continue;
                        for (WavePoint& wp : wave.points) {
                            if (glm::length(p.pos - wp.localPos) < 20.0f) {
                                wave.energy = 0.0f;
                                p.n += 1;
                                p.excitedTimer += 0.003f;
                                goto next_particle;
                            }
                        }
                    }
                next_particle:;
                }
            }
        }

        for (auto it = waves.begin(); it != waves.end(); ) {
            if (it->update(0.01f, engine.WIDTH, engine.HEIGHT))
                it = waves.erase(it);
            else
                ++it;
        }

        engine.render(atoms, waves);
        glfwPollEvents();
    }

    engine.vertexBuffer->release();
    engine.uniformBuffer->release();
    if (engine.pipeline) engine.pipeline->release();
    engine.commandQueue->release();
    engine.device->release();
    glfwDestroyWindow(engine.window);
    glfwTerminate();
    return 0;
}
