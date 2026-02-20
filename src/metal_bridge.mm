#import <QuartzCore/CAMetalLayer.h>
#import <Metal/Metal.h>
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

extern "C" void* attachMetalLayer(void* glfwWindow, void* mtlDevice) {
    NSWindow* nswindow = glfwGetCocoaWindow((GLFWwindow*)glfwWindow);
    CAMetalLayer* layer = [CAMetalLayer layer];
    layer.device = (__bridge id<MTLDevice>)mtlDevice;
    layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    nswindow.contentView.layer = layer;
    nswindow.contentView.wantsLayer = YES;
    return (__bridge void*)layer;
}
