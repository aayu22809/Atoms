#define NS_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#include <Foundation/Foundation.hpp>
#include <QuartzCore/QuartzCore.hpp>
#include <Metal/Metal.hpp>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <CoreGraphics/CoreGraphics.h>

extern "C" void* attachMetalLayer(void*, void*);

int main() {
    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    GLFWwindow* window = glfwCreateWindow(800, 600, "Metal Test", nullptr, nullptr);
    if (!window) { glfwTerminate(); return -1; }

    MTL::Device* device = MTL::CreateSystemDefaultDevice();
    if (!device) { glfwDestroyWindow(window); glfwTerminate(); return -1; }

    CA::MetalLayer* metalLayer = (CA::MetalLayer*)attachMetalLayer(window, device);
    if (!metalLayer) { device->release(); glfwDestroyWindow(window); glfwTerminate(); return -1; }

    MTL::CommandQueue* commandQueue = device->newCommandQueue();

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        if (width > 0 && height > 0) {
            metalLayer->setDrawableSize(CGSizeMake(width, height));
            CA::MetalDrawable* drawable = metalLayer->nextDrawable();
            if (drawable) {
                MTL::RenderPassDescriptor* passDesc = MTL::RenderPassDescriptor::renderPassDescriptor();
                passDesc->colorAttachments()->object(0)->setTexture(drawable->texture());
                passDesc->colorAttachments()->object(0)->setLoadAction(MTL::LoadActionClear);
                passDesc->colorAttachments()->object(0)->setStoreAction(MTL::StoreActionStore);
                passDesc->colorAttachments()->object(0)->setClearColor(MTL::ClearColor::Make(0.2, 0.4, 0.8, 1.0));

                MTL::CommandBuffer* cmdBuf = commandQueue->commandBuffer();
                MTL::RenderCommandEncoder* enc = cmdBuf->renderCommandEncoder(passDesc);
                enc->endEncoding();
                cmdBuf->presentDrawable(drawable);
                cmdBuf->commit();
            }
        }

        pool->release();
    }

    commandQueue->release();
    device->release();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
