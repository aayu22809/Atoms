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
#include <mach-o/dyld.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

extern "C" void* attachMetalLayer(void*, void*);

float orbitDistance = 200.0f;

struct Particle {
    glm::vec2 pos;
    int charge;
    float angle = M_PI;
    float energy = -13.6f;
    int n = 1;
    float excitedTimer = 0.0f;
    Particle(glm::vec2 pos, int charge) : pos(pos), charge(charge) {}

    void update(glm::vec2 c) {
        float numOscillations = 0;
        if (energy < 0) numOscillations = -13.6f / energy;
        float baseOrbit = orbitDistance;
        float amplitude = 50.0f;
        float r = baseOrbit + amplitude * sin(numOscillations * angle);
        angle += 0.05f;
        pos = glm::vec2(cos(angle) * r + c.x, sin(angle) * r + c.y);
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

static std::string getShaderSourcePath() {
    char path[1024];
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) == 0) {
        std::string exe(path);
        size_t pos = exe.find_last_of("/");
        if (pos != std::string::npos) {
            return exe.substr(0, pos) + "/../shaders/quad_2d.metal";
        }
    }
    return "shaders/quad_2d.metal";
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
    MTL::Buffer* vertexBuffer = nullptr;
    MTL::Buffer* uniformBuffer = nullptr;
    static const size_t MAX_VERTICES = 100000;

    Engine() {
        if (!glfwInit()) { std::cerr << "GLFW init failed\n"; exit(EXIT_FAILURE); }
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow(WIDTH, HEIGHT, "2D Wave Atom - Metal", nullptr, nullptr);
        if (!window) { glfwTerminate(); exit(EXIT_FAILURE); }

        device = MTL::CreateSystemDefaultDevice();
        if (!device) { glfwDestroyWindow(window); glfwTerminate(); exit(EXIT_FAILURE); }

        metalLayer = (CA::MetalLayer*)attachMetalLayer(window, device);
        if (!metalLayer) { device->release(); glfwDestroyWindow(window); glfwTerminate(); exit(EXIT_FAILURE); }

        commandQueue = device->newCommandQueue();

        NS::Error* err = nullptr;
        std::string srcPath = getShaderSourcePath();
        std::string src = loadFile(srcPath);
        MTL::Library* lib = nullptr;
        if (!src.empty()) {
            lib = device->newLibrary(NS::String::string(src.c_str(), NS::UTF8StringEncoding), nullptr, &err);
        }
        if (!lib) {
            std::cerr << "Failed to compile Metal shader from: " << srcPath << "\n";
            if (err && err->localizedDescription()) std::cerr << "Error: " << err->localizedDescription()->utf8String() << "\n";
        }
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

    void buildVertices(std::vector<float>& vertices, size_t& lineCount, size_t& triCount, std::vector<Atom>& atoms) {
        vertices.clear();
        lineCount = 0;
        triCount = 0;

        for (Atom& a : atoms) {
            for (Particle& p : a.particles) {
                if (p.charge == -1) {
                    float numOsc = -13.6f / p.energy;
                    float baseOrbit = orbitDistance, amplitude = 50.0f;
                    int segs = 5000;
                    for (int i = 0; i <= segs; i++) {
                        float ang = 2.0f * (float)M_PI * i / segs;
                        float r = baseOrbit + amplitude * sin(numOsc * ang);
                        float x = cos(ang) * r + a.pos.x;
                        float y = sin(ang) * r + a.pos.y;
                        vertices.insert(vertices.end(), {x, y, 0.4f, 0.4f, 0.4f, 1.0f});
                    }
                    lineCount += segs + 1;
                }
                float rad = (p.charge == -1) ? 10.0f : 50.0f;
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

    void render(std::vector<Atom>& atoms) {
        if (!pipeline) return;

        std::vector<float> vertices;
        size_t lineCount, triCount;
        buildVertices(vertices, lineCount, triCount, atoms);
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

        if (lineCount > 0) {
            enc->drawPrimitives(MTL::PrimitiveTypeLine, NS::UInteger(0), NS::UInteger(lineCount));
        }
        if (triCount > 0) {
            enc->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(lineCount), NS::UInteger(triCount));
        }

        enc->endEncoding();
        cmdBuf->presentDrawable(drawable);
        cmdBuf->commit();

        pool->release();
    }
};

static Engine* g_engine = nullptr;
static std::vector<Atom> atoms{Atom(glm::vec2(0.0f, 0.0f))};

void key_callback(GLFWwindow*, int key, int, int action, int) {
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;
    float d = 0.0f;
    if (key == GLFW_KEY_W) d = 0.01f;
    else if (key == GLFW_KEY_S) d = -0.01f;
    else if (key == GLFW_KEY_E) d = 0.1f;
    else if (key == GLFW_KEY_D) d = -0.1f;
    else if (key == GLFW_KEY_R) d = 1.0f;
    else if (key == GLFW_KEY_F) d = -1.0f;
    if (d != 0.0f) {
        for (Atom& a : atoms) {
            for (Particle& p : a.particles) {
                p.energy += d;
                if (key == GLFW_KEY_W) p.angle = 0;
                std::cout << "Particle energy: " << p.energy << "\n";
            }
        }
    }
}

int main() {
    Engine engine;
    g_engine = &engine;
    glfwSetWindowUserPointer(engine.window, &engine);
    glfwSetKeyCallback(engine.window, key_callback);

    while (!glfwWindowShouldClose(engine.window)) {
        for (Atom& a : atoms) {
            for (Particle& p : a.particles) {
                if (p.charge == -1) p.update(a.pos);
            }
        }
        engine.render(atoms);
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
