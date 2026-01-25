#include "VulkanRenderer.h"
#include <vulkan/vulkan.h>
#include <SDL.h>
#include <SDL_vulkan.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <iostream>
#include <cstring>
#include <string>
#include <stdexcept>
#include <algorithm>

#include <shaderc/shaderc.hpp>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/error.h>
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}
// #include "playerDataRpc.h"
#include "globalData.h"
// === GLSL sources ===
// Driver conversion fragment (single combined sampler)
static const char* kFragYcbcrGLSL = R"glsl(
#version 450
layout(binding = 0) uniform sampler2D tex; // combined sampler with YCbCr conversion
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;
void main() {
    outColor = texture(tex, uv);
}
)glsl";

// Manual NV12 -> RGB (two samplers: Y and UV)
static const char* kFragManualGLSL = R"glsl(
#version 450
layout(binding = 0) uniform sampler2D texY;
layout(binding = 1) uniform sampler2D texUV;
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;
void main() {
    float y = texture(texY, uv).r;
    vec2 uvSample = texture(texUV, uv).rg;
    float u = uvSample.r - 0.5;
    float v = uvSample.g - 0.5;
    // BT.601 full range-ish conversion; adjust constants if needed
    float r = y + 1.402 * v;
    float g = y - 0.344136 * u - 0.714136 * v;
    float b = y + 1.772 * u;
    outColor = vec4(r, g, b, 1.0);
}
)glsl";

static const char* kVertexGLSL = R"glsl(
#version 450
layout(location = 0) out vec2 uv;
void main() {
    vec2 pos[3] = vec2[](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0)
    );
    vec2 p = pos[gl_VertexIndex];
    uv = p * 0.5 + 0.5;
    gl_Position = vec4(p, 0.0, 1.0);
}
)glsl";



void  VulkanRenderer:: init(SDL_Window* win)
{
    window = win;
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createDeviceAndQueues();
    if (device == VK_NULL_HANDLE) {
        throw std::runtime_error("Vulkan device is VK_NULL_HANDLE after init!");
    }
    createSwapchain();
    createCommandPool();
    createRenderResources();

    for (auto it = m_drawCallbacks.begin (); it != m_drawCallbacks.end (); it++) {
        (*it)->onInit();
    }
}

void  VulkanRenderer:: cleanup()
{
    for (auto it = m_drawCallbacks.begin (); it != m_drawCallbacks.end (); it++) {
        (*it)->cleanup();
    }
    if (device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device);
        cleanupRenderResources();
        for (auto fb : swapFramebuffers) vkDestroyFramebuffer(device, fb, nullptr);
        for (auto view : swapViews) vkDestroyImageView(device, view, nullptr);
        if (swapchain != VK_NULL_HANDLE) vkDestroySwapchainKHR(device, swapchain, nullptr);
        if (cmdPool != VK_NULL_HANDLE) vkDestroyCommandPool(device, cmdPool, nullptr);
        vkDestroyDevice(device, nullptr);
    }
    if (surface != VK_NULL_HANDLE) vkDestroySurfaceKHR(instance, surface, nullptr);
    if (debugMessenger != VK_NULL_HANDLE) destroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
    if (instance != VK_NULL_HANDLE) vkDestroyInstance(instance, nullptr);

}
VulkanRenderer::renderState   VulkanRenderer:: state()
{
    return m_renderState;
}

void          VulkanRenderer:: setState(renderState st)
{
    m_renderState = st;
}

bool  VulkanRenderer:: presentNV12(const videoFrameData& vf)
{
        if (VulkanRenderer::renderState_OK != state()) [[unlikely]]{
            return true;
        }
        if (device == VK_NULL_HANDLE) [[unlikely]]return false;
        if (!vf.m_planNum || extent.width == 0 || extent.height == 0) [[unlikely]]return false;

        if (m_curVideoWidth != vf.m_width || m_curVideoHeight != vf.m_height) {
            m_curVideoWidth = vf.m_width;
            m_curVideoHeight = vf.m_height;
            recreateSwapchain();
        }
        VulkanRenderResources::Frame* frame = nullptr;
        for (auto& f : renderRes.frames) {
            if (!f.inFlight) { frame = &f; break; }
        }
        if (!frame) [[unlikely]]return false;
        frame->inFlight = true;

        uint32_t yw = (uint32_t)vf.m_width;
        uint32_t yh = (uint32_t)vf.m_height;
        uint32_t uvw = (uint32_t)((vf.m_width + 1) / 2);
        uint32_t uvh = (uint32_t)((vf.m_height + 1) / 2);

        // recreate images if size changed
        if (m_ycbcrFeatureEnabled) {
            // use multi-planar image
            if (!frame->image || frame->imageExtent.width != yw || frame->imageExtent.height != yh) {
                if (frame->imageView) { vkDestroyImageView(device, frame->imageView, nullptr); frame->imageView = VK_NULL_HANDLE; }
                if (frame->image) { vkDestroyImage(device, frame->image, nullptr); frame->image = VK_NULL_HANDLE; }
                if (frame->imageMem) { vkFreeMemory(device, frame->imageMem, nullptr); frame->imageMem = VK_NULL_HANDLE; }
                createImageNV12Multi(yw, yh, frame->image, frame->imageMem, frame->imageView);
                frame->imageExtent = { yw, yh };
            }
        }
        else {
            // manual: create/resize per-plane single images
            if (!frame->imageY || frame->imageExtent.width != yw || frame->imageExtent.height != yh) {
                if (frame->imageViewY) { vkDestroyImageView(device, frame->imageViewY, nullptr); frame->imageViewY = VK_NULL_HANDLE; }
                if (frame->imageY) { vkDestroyImage(device, frame->imageY, nullptr); frame->imageY = VK_NULL_HANDLE; }
                if (frame->imageYMem) { vkFreeMemory(device, frame->imageYMem, nullptr); frame->imageYMem = VK_NULL_HANDLE; }

                if (frame->imageViewUV) { vkDestroyImageView(device, frame->imageViewUV, nullptr); frame->imageViewUV = VK_NULL_HANDLE; }
                if (frame->imageUV) { vkDestroyImage(device, frame->imageUV, nullptr); frame->imageUV = VK_NULL_HANDLE; }
                if (frame->imageUVMem) { vkFreeMemory(device, frame->imageUVMem, nullptr); frame->imageUVMem = VK_NULL_HANDLE; }

                createImageSinglePlane(yw, yh, VK_FORMAT_R8_UNORM, frame->imageY, frame->imageYMem, frame->imageViewY);
                createImageSinglePlane(uvw, uvh, VK_FORMAT_R8G8_UNORM, frame->imageUV, frame->imageUVMem, frame->imageViewUV);
                frame->imageExtent = { yw, yh };
            }
        }

        // create staging buffers and upload
        VkBuffer stagingY = VK_NULL_HANDLE; VkDeviceMemory stagingYMem = VK_NULL_HANDLE;
        VkBuffer stagingUV = VK_NULL_HANDLE; VkDeviceMemory stagingUVMem = VK_NULL_HANDLE;

        VkDeviceSize ySize = (VkDeviceSize)vf.m_y_planesize;
        createBuffer(ySize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingY, stagingYMem);
        if (stagingY == VK_NULL_HANDLE) { frame->inFlight = false; return false; }
        void* mapPtr = nullptr;
        if (vkMapMemory(device, stagingYMem, 0, ySize, 0, &mapPtr) != VK_SUCCESS) {
            vkDestroyBuffer(device, stagingY, nullptr); vkFreeMemory(device, stagingYMem, nullptr);
            frame->inFlight = false; return false;
        }
        auto pPlaneData = vf.m_plan.get(); //&vf.m_plan[0];
        for (int row = 0; row < vf.m_height; ++row) {
            memcpy((uint8_t*)mapPtr + (size_t)row * vf.m_y_linesize, pPlaneData + (size_t)row * vf.m_y_linesize, vf.m_y_linesize);
        }
        vkUnmapMemory(device, stagingYMem);
        auto pUVPlaneData = pPlaneData + vf.m_y_planesize;
        VkDeviceSize uvSize = (VkDeviceSize)(vf.m_planNum - vf.m_y_planesize);
        createBuffer(uvSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingUV, stagingUVMem);
        if (stagingUV == VK_NULL_HANDLE) {
            vkDestroyBuffer(device, stagingY, nullptr); vkFreeMemory(device, stagingYMem, nullptr);
            frame->inFlight = false; return false;
        }
        if (vkMapMemory(device, stagingUVMem, 0, uvSize, 0, &mapPtr) != VK_SUCCESS) {
            vkDestroyBuffer(device, stagingY, nullptr); vkFreeMemory(device, stagingYMem, nullptr);
            vkDestroyBuffer(device, stagingUV, nullptr); vkFreeMemory(device, stagingUVMem, nullptr);
            frame->inFlight = false; return false;
        }
        for (int row = 0; row < (vf.m_height + 1) / 2; ++row) {
            memcpy((uint8_t*)mapPtr + (size_t)row * vf.m_uv_linesize, pUVPlaneData + (size_t)row * vf.m_uv_linesize, vf.m_uv_linesize);
        }
        vkUnmapMemory(device, stagingUVMem);

        VkCommandBuffer uploadCmd = beginSingleTimeCommands();

        if (m_ycbcrFeatureEnabled) {
            // per-plane transitions
            transitionImageLayout(uploadCmd, frame->image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_PLANE_0_BIT);
            transitionImageLayout(uploadCmd, frame->image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_PLANE_1_BIT);

            VkBufferImageCopy regionY{};
            regionY.bufferOffset = 0;
            regionY.bufferRowLength = 0; // tightly packed
            regionY.bufferImageHeight = 0;
            regionY.imageSubresource.aspectMask = VK_IMAGE_ASPECT_PLANE_0_BIT;
            regionY.imageSubresource.mipLevel = 0;
            regionY.imageSubresource.baseArrayLayer = 0;
            regionY.imageSubresource.layerCount = 1;
            regionY.imageOffset = { 0, 0, 0 };
            regionY.imageExtent = { yw, yh, 1 };
            vkCmdCopyBufferToImage(uploadCmd, stagingY, frame->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &regionY);

            VkBufferImageCopy regionUV{};
            regionUV.bufferOffset = 0;
            regionUV.bufferRowLength = 0; // tightly packed
            regionUV.bufferImageHeight = 0;
            regionUV.imageSubresource.aspectMask = VK_IMAGE_ASPECT_PLANE_1_BIT;
            regionUV.imageSubresource.mipLevel = 0;
            regionUV.imageSubresource.baseArrayLayer = 0;
            regionUV.imageSubresource.layerCount = 1;
            regionUV.imageOffset = { 0, 0, 0 };
            regionUV.imageExtent = { uvw, uvh, 1 };
            vkCmdCopyBufferToImage(uploadCmd, stagingUV, frame->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &regionUV);

            transitionImageLayout(uploadCmd, frame->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_PLANE_0_BIT);
            transitionImageLayout(uploadCmd, frame->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_PLANE_1_BIT);
        }
        else {
            // manual: upload to single-plane images
            transitionImageLayout(uploadCmd, frame->imageY, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            transitionImageLayout(uploadCmd, frame->imageUV, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

            VkBufferImageCopy rY{};
            rY.bufferOffset = 0;
            rY.bufferRowLength = 0; // tightly packed
            rY.bufferImageHeight = 0;
            rY.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            rY.imageSubresource.mipLevel = 0;
            rY.imageSubresource.baseArrayLayer = 0;
            rY.imageSubresource.layerCount = 1;
            rY.imageOffset = { 0,0,0 };
            rY.imageExtent = { yw, yh, 1 };
            vkCmdCopyBufferToImage(uploadCmd, stagingY, frame->imageY, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &rY);

            VkBufferImageCopy rUV{};
            rUV.bufferOffset = 0;
            rUV.bufferRowLength = 0; // tightly packed
            rUV.bufferImageHeight = 0;
            rUV.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            rUV.imageSubresource.mipLevel = 0;
            rUV.imageSubresource.baseArrayLayer = 0;
            rUV.imageSubresource.layerCount = 1;
            rUV.imageOffset = { 0,0,0 };
            rUV.imageExtent = { uvw, uvh, 1 };
            vkCmdCopyBufferToImage(uploadCmd, stagingUV, frame->imageUV, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &rUV);

            transitionImageLayout(uploadCmd, frame->imageY, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            transitionImageLayout(uploadCmd, frame->imageUV, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }

        endSingleTimeCommands(uploadCmd);

        vkDestroyBuffer(device, stagingY, nullptr); vkFreeMemory(device, stagingYMem, nullptr);
        vkDestroyBuffer(device, stagingUV, nullptr); vkFreeMemory(device, stagingUVMem, nullptr);

        // update descriptor(s)
        if (frame->descSet != VK_NULL_HANDLE) {
            if (m_ycbcrFeatureEnabled) {
                // descriptor layout was created WITH immutable sampler -> set sampler = VK_NULL_HANDLE
                VkDescriptorImageInfo di{};
                di.sampler = VK_NULL_HANDLE; // immutable sampler is bound in the layout
                di.imageView = frame->imageView;
                di.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                VkWriteDescriptorSet wds{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
                wds.dstSet = frame->descSet;
                wds.dstBinding = 0;
                wds.dstArrayElement = 0;
                wds.descriptorCount = 1;
                wds.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                wds.pImageInfo = &di;
                vkUpdateDescriptorSets(device, 1, &wds, 0, nullptr);
            }
            else {
                VkDescriptorImageInfo diY{};
                diY.sampler = renderRes.sampler;
                diY.imageView = frame->imageViewY;
                diY.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                VkDescriptorImageInfo diUV{};
                diUV.sampler = renderRes.sampler;
                diUV.imageView = frame->imageViewUV;
                diUV.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                VkWriteDescriptorSet wds[2]{};
                wds[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                wds[0].dstSet = frame->descSet;
                wds[0].dstBinding = 0;
                wds[0].dstArrayElement = 0;
                wds[0].descriptorCount = 1;
                wds[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                wds[0].pImageInfo = &diY;
                wds[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                wds[1].dstSet = frame->descSet;
                wds[1].dstBinding = 1;
                wds[1].dstArrayElement = 0;
                wds[1].descriptorCount = 1;
                wds[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                wds[1].pImageInfo = &diUV;
                vkUpdateDescriptorSets(device, 2, wds, 0, nullptr);
            }
        }

        // Acquire, record draw and present
        uint32_t imageIndex;
        VkResult r = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, frame->imageAvailable, VK_NULL_HANDLE, &imageIndex);
        if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR) {
            recreateSwapchain();
            frame->inFlight = false;
            return false;
        }

        if (r != VK_SUCCESS) {
            frame->inFlight = false;
            return false;
        }

        VkCommandBuffer cmd = frame->cmdBuf;
        if (cmd == VK_NULL_HANDLE) { frame->inFlight = false; return false; }
        if (vkResetCommandBuffer(cmd, 0) != VK_SUCCESS) { frame->inFlight = false; return false; }

        VkCommandBufferBeginInfo bi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if (vkBeginCommandBuffer(cmd, &bi) != VK_SUCCESS) { frame->inFlight = false; return false; }

        VkRenderPassBeginInfo rpInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
        rpInfo.renderPass = renderRes.renderPass;
        rpInfo.framebuffer = swapFramebuffers[imageIndex];
        rpInfo.renderArea.offset = { 0, 0 };
        rpInfo.renderArea.extent = extent;
        VkClearValue clear = {0.0f, 0.0f, 0.0f, 1.0f};
        rpInfo.clearValueCount = 1;
        rpInfo.pClearValues = &clear;

        vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, renderRes.pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, renderRes.pipelineLayout, 0, 1, &frame->descSet, 0, nullptr);
        vkCmdDraw(cmd, 3, 1, 0, 0);

        for (auto it = m_drawCallbacks.begin (); it != m_drawCallbacks.end (); it++) {
            (*it)->onDraw (cmd);
        }
        vkCmdEndRenderPass(cmd);
        vkEndCommandBuffer(cmd);

        VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &frame->imageAvailable;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmd;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &frame->renderFinished;

        if (frame->fence == VK_NULL_HANDLE) {
            VkFenceCreateInfo fci{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
            fci.flags = 0;
            vkCreateFence(device, &fci, nullptr, &frame->fence);
        }
        vkResetFences(device, 1, &frame->fence);
        if (vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, frame->fence) != VK_SUCCESS) {
            frame->inFlight = false;
            return false;
        }

        VkPresentInfoKHR presentInfo{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &frame->renderFinished;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swapchain;
        presentInfo.pImageIndices = &imageIndex;
        VkResult presRes = vkQueuePresentKHR(presentQueue, &presentInfo);
        if (presRes == VK_ERROR_OUT_OF_DATE_KHR || presRes == VK_SUBOPTIMAL_KHR) recreateSwapchain();

        vkWaitForFences(device, 1, &frame->fence, VK_TRUE, UINT64_MAX);
        frame->inFlight = false;
        return true;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallbackFn(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageTypes,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData) {
    (void)messageSeverity; (void)messageTypes; (void)pUserData;
    std::cerr << "VULKAN VALIDATION: " << (pCallbackData->pMessage ? pCallbackData->pMessage : "") << std::endl;
    return VK_FALSE;
}

VkResult  VulkanRenderer:: createDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pMessenger)
{
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
        if (func != nullptr) {
            return func(instance, pCreateInfo, pAllocator, pMessenger);
        }
        else {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
}

void  VulkanRenderer:: destroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT messenger, const VkAllocationCallbacks* pAllocator)
{

        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr) func(instance, messenger, pAllocator);
}

void  VulkanRenderer:: populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& info)
{
info = VkDebugUtilsMessengerCreateInfoEXT{};
        info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        info.pfnUserCallback = debugCallbackFn;
        info.pUserData = nullptr;
}

void  VulkanRenderer:: setupDebugMessenger()
{
if (!enableValidation) return;
        VkDebugUtilsMessengerCreateInfoEXT ci;
        populateDebugMessengerCreateInfo(ci);
        if (createDebugUtilsMessengerEXT(instance, &ci, nullptr, &debugMessenger) != VK_SUCCESS) {
            std::cerr << "Warning: failed to set up debug messenger\n";
        }
}

bool  VulkanRenderer:: checkLayerAvailable(const char* layerName)
{
uint32_t count = 0;
        vkEnumerateInstanceLayerProperties(&count, nullptr);
        std::vector<VkLayerProperties> props(count);
        if (count) vkEnumerateInstanceLayerProperties(&count, props.data());
        for (auto& p : props) {
            if (strcmp(p.layerName, layerName) == 0) return true;
        }
        return false;
}

void  VulkanRenderer:: createSurface()
{

        if (!SDL_Vulkan_CreateSurface(window, instance, &surface)) {
            throw std::runtime_error("SDL_Vulkan_CreateSurface failed");
        }
}

void  VulkanRenderer:: createInstance()
{
unsigned extCount = 0;
        SDL_Vulkan_GetInstanceExtensions(window, &extCount, nullptr);
        std::vector<const char*> exts(extCount);
        SDL_Vulkan_GetInstanceExtensions(window, &extCount, exts.data());
        if (enableValidation) exts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

        // ensure instance provides VK_KHR_get_physical_device_properties2 if available on loader
        uint32_t availCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &availCount, nullptr);
        if (availCount > 0) {
            std::vector<VkExtensionProperties> avail(availCount);
            vkEnumerateInstanceExtensionProperties(nullptr, &availCount, avail.data());
            for (auto& e : avail) {
                if (strcmp(e.extensionName, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME) == 0) {
                    bool already = false;
                    for (auto a : exts) if (strcmp(a, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME) == 0) { already = true; break; }
                    if (!already) exts.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
                    break;
                }
            }
        }

        VkApplicationInfo appInfo{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
        appInfo.pApplicationName = "Player";
        appInfo.apiVersion = VK_API_VERSION_1_1;

        VkInstanceCreateInfo instInfo{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
        instInfo.pApplicationInfo = &appInfo;
        instInfo.enabledExtensionCount = (uint32_t)exts.size();
        instInfo.ppEnabledExtensionNames = exts.data();

        const char* validationLayer = "VK_LAYER_KHRONOS_validation";
        std::vector<const char*> layers;
        if (enableValidation && checkLayerAvailable(validationLayer)) {
            layers.push_back(validationLayer);
        }
        else {
            enableValidation = false;
        }
        instInfo.enabledLayerCount = (uint32_t)layers.size();
        instInfo.ppEnabledLayerNames = layers.empty() ? nullptr : layers.data();

        VkDebugUtilsMessengerCreateInfoEXT dbgCreateInfo{};
        if (enableValidation) {
            populateDebugMessengerCreateInfo(dbgCreateInfo);
            instInfo.pNext = &dbgCreateInfo;
        }
        else instInfo.pNext = nullptr;

        VkResult r = vkCreateInstance(&instInfo, nullptr, &instance);
        if (r != VK_SUCCESS) {
            std::cerr << "vkCreateInstance(VK 1.1) failed, retrying with VK_API_VERSION_1_0\n";
            appInfo.apiVersion = VK_API_VERSION_1_0;
            instInfo.pApplicationInfo = &appInfo;
            instInfo.pNext = enableValidation ? &dbgCreateInfo : nullptr;
            r = vkCreateInstance(&instInfo, nullptr, &instance);
            if (r != VK_SUCCESS) {
                throw std::runtime_error(std::string("vkCreateInstance failed: ") + std::to_string(r));
            }
        }
}

void  VulkanRenderer:: pickPhysicalDevice()
{
    uint32_t devCount = 0;
    vkEnumeratePhysicalDevices(instance, &devCount, nullptr);
    if (devCount == 0) throw std::runtime_error("no vulkan devices");
    std::vector<VkPhysicalDevice> devices(devCount);
    vkEnumeratePhysicalDevices(instance, &devCount, devices.data());
    for (auto d : devices) {
        if (isDeviceSuitable(d)) { physical = d; break; }
    }
    if (physical == VK_NULL_HANDLE) throw std::runtime_error("no suitable device");

    // Print diagnostics
    /*
    {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(physical, &props);
        std::cerr << "Selected physical device: " << props.deviceName << " (apiVersion: "
            << VK_VERSION_MAJOR(props.apiVersion) << "."
            << VK_VERSION_MINOR(props.apiVersion) << "."
            << VK_VERSION_PATCH(props.apiVersion) << ")\n";
    }
    uint32_t extCount = 0;
    vkEnumerateDeviceExtensionProperties(physical, nullptr, &extCount, nullptr);
    std::vector<VkExtensionProperties> exts(extCount);
    if (extCount) vkEnumerateDeviceExtensionProperties(physical, nullptr, &extCount, exts.data());
    std::cerr << "Device supports " << extCount << " extensions:\n";
    for (auto& e : exts) {
        std::cerr << "  " << e.extensionName << " (spec " << e.specVersion << ")\n";
    }
    */
}

bool  VulkanRenderer:: isDeviceSuitable(VkPhysicalDevice d)
{
    uint32_t qCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(d, &qCount, nullptr);
    std::vector<VkQueueFamilyProperties> qProps(qCount);
    vkGetPhysicalDeviceQueueFamilyProperties(d, &qCount, qProps.data());
    for (uint32_t i = 0; i < qCount; ++i) {
        if (qProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(d, i, surface, &presentSupport);
            if (presentSupport) {
                m_graphicsQueueFamily = i;
                presentQueueFamily = i;
                return true;
            }
        }
    }
    return false;
}

bool  VulkanRenderer:: deviceExtensionSupported(VkPhysicalDevice dev, const char* extName)
{
    uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> exts(count);
    if (count) vkEnumerateDeviceExtensionProperties(dev, nullptr, &count, exts.data());
    for (auto& e : exts) if (strcmp(e.extensionName, extName) == 0) return true;
    return false;
}

void  VulkanRenderer:: createDeviceAndQueues()
{
float prio = 1.0f;
        VkDeviceQueueCreateInfo qci{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
        qci.queueFamilyIndex = m_graphicsQueueFamily;
        qci.queueCount = 1;
        qci.pQueuePriorities = &prio;

        // base required extension
        std::vector<const char*> deviceExts;
        deviceExts.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

        // Query physical device properties to detect Vulkan API version (some extensions promoted to core in 1.1)
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(physical, &props);
        bool physSupportsVulkan11 = (props.apiVersion >= VK_API_VERSION_1_1);

        // check if physical device supports sampler-ycbcr-conversion extension
        bool wantYcbcr = deviceExtensionSupported(physical, VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME);
        bool allDepsPresent = true;
        if (wantYcbcr) {
            // dependencies required by VK_KHR_sampler_ycbcr_conversion
            const char* deps[] = {
                VK_KHR_MAINTENANCE1_EXTENSION_NAME,
                VK_KHR_BIND_MEMORY_2_EXTENSION_NAME,
                VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
                VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
            };
            for (auto d : deps) {
                if (strcmp(d, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME) == 0) {
                    // this one may be promoted to core in Vulkan 1.1
                    if (physSupportsVulkan11) {
                        // treat as present (do not add to deviceExts)
                        continue;
                    }
                    // else fall through to device extension check
                }
                if (deviceExtensionSupported(physical, d)) {
                    deviceExts.push_back(d);
                }
                else {
                    std::cerr << "device missing dependency for ycbcr conversion: " << d << "\n";
                    allDepsPresent = false;
                }
            }
            if (allDepsPresent) {
                deviceExts.push_back(VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME);
            }
            else {
                std::cerr << "Warning: missing required dependencies for " << VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME << ", disabling YCbCr conversion\n";
                wantYcbcr = false;
            }
        }

        // enable samplerYcbcrConversion feature only if everything supported
        VkPhysicalDeviceSamplerYcbcrConversionFeatures ycbcrFeat{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES };
        ycbcrFeat.pNext = nullptr;
        ycbcrFeat.samplerYcbcrConversion = wantYcbcr ? VK_TRUE : VK_FALSE;
        m_ycbcrFeatureEnabled = wantYcbcr && allDepsPresent;

        VkPhysicalDeviceFeatures features{};
        VkDeviceCreateInfo dci{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
        dci.queueCreateInfoCount = 1;
        dci.pQueueCreateInfos = &qci;
        dci.enabledExtensionCount = (uint32_t)deviceExts.size();
        dci.ppEnabledExtensionNames = deviceExts.data();
        dci.pEnabledFeatures = &features;
        dci.pNext = ycbcrFeatureEnabled() ? (void*)&ycbcrFeat : nullptr;

        VkResult res = vkCreateDevice(physical, &dci, nullptr, &device);
        if (res != VK_SUCCESS) {
            device = VK_NULL_HANDLE;
            throw std::runtime_error("vkCreateDevice failed! Code: " + std::to_string(res));
        }

        vkGetDeviceQueue(device, m_graphicsQueueFamily, 0, &m_graphicsQueue);
        vkGetDeviceQueue(device, presentQueueFamily, 0, &presentQueue);
}

void  VulkanRenderer:: createSwapchain()
{
VkSurfaceCapabilitiesKHR caps{};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical, surface, &caps);
        uint32_t formatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &formatCount, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        if (formatCount > 0) vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &formatCount, formats.data());
        VkSurfaceFormatKHR chosenFormat = formats.size() ? formats[0] : VkSurfaceFormatKHR{ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
        for (const auto& f : formats) {
            if (f.format == VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                chosenFormat = f; break;
            }
        }
        swapchainFormat = chosenFormat.format;
        uint32_t presentModeCount = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(physical, surface, &presentModeCount, nullptr);
        std::vector<VkPresentModeKHR> presentModes(presentModeCount);
        if (presentModeCount > 0) vkGetPhysicalDeviceSurfacePresentModesKHR(physical, surface, &presentModeCount, presentModes.data());
        VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
        for (auto pm : presentModes) {
            if (pm == VK_PRESENT_MODE_MAILBOX_KHR) { presentMode = pm; break; }
        }
        int w, h;
        SDL_Vulkan_GetDrawableSize(window, &w, &h);
        extent.width = std::max(1u, (uint32_t)w);
        extent.height = std::max(1u, (uint32_t)h);
        extent.width = std::max(caps.minImageExtent.width, std::min(caps.maxImageExtent.width, extent.width));
        extent.height = std::max(caps.minImageExtent.height, std::min(caps.maxImageExtent.height, extent.height));
        uint32_t imageCount = caps.maxImageCount ? std::min(caps.maxImageCount, std::max(caps.minImageCount + 1, 2u)) : std::max(caps.minImageCount + 1, 2u);
        m_minImageCount = caps.minImageCount;
        m_imageCount = imageCount;
        VkSwapchainCreateInfoKHR sci{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
        sci.surface = surface;
        sci.minImageCount = imageCount;
        sci.imageFormat = swapchainFormat;
        sci.imageColorSpace = chosenFormat.colorSpace;
        sci.imageExtent = extent;
        sci.imageArrayLayers = 1;
        sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        sci.preTransform = caps.currentTransform;
        sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        sci.presentMode = presentMode;
        sci.clipped = VK_TRUE;
        if (vkCreateSwapchainKHR(device, &sci, nullptr, &swapchain) != VK_SUCCESS) {
            throw std::runtime_error("vkCreateSwapchainKHR failed");
        }
        uint32_t count;
        vkGetSwapchainImagesKHR(device, swapchain, &count, nullptr);
        swapImages.resize(count);
        vkGetSwapchainImagesKHR(device, swapchain, &count, swapImages.data());
        swapViews.resize(count);
        for (size_t i = 0; i < count; ++i) {
            VkImageViewCreateInfo ivci{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
            ivci.image = swapImages[i];
            ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
            ivci.format = swapchainFormat;
            ivci.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            ivci.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            ivci.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            ivci.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            ivci.subresourceRange.baseMipLevel = 0;
            ivci.subresourceRange.levelCount = 1;
            ivci.subresourceRange.baseArrayLayer = 0;
            ivci.subresourceRange.layerCount = 1;
            vkCreateImageView(device, &ivci, nullptr, &swapViews[i]);
        }
}

void  VulkanRenderer:: createCommandPool()
{
    VkCommandPoolCreateInfo cpi{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    cpi.queueFamilyIndex = m_graphicsQueueFamily;
    cpi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(device, &cpi, nullptr, &cmdPool) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateCommandPool failed");
    }
}

std::vector<uint32_t>  VulkanRenderer:: compileGLSLtoSPV(const std::string& source, shaderc_shader_kind kind, const char* source_name)
{
    shaderc::Compiler compiler;
    shaderc::CompileOptions options;
    options.SetOptimizationLevel(shaderc_optimization_level_performance);
    shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(source, kind, source_name, options);
    if (module.GetCompilationStatus() != shaderc_compilation_status_success) {
        throw std::runtime_error(std::string("shaderc compile error: ") + module.GetErrorMessage());
    }
    return std::vector<uint32_t>(module.cbegin(), module.cend());
}

    // helper: create single-plane image + view
void  VulkanRenderer:: createImageSinglePlane(uint32_t width, uint32_t height, VkFormat fmt, VkImage& image, VkDeviceMemory& memory, VkImageView& view)
{
if (device == VK_NULL_HANDLE) return;
        VkImageCreateInfo ici{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        ici.imageType = VK_IMAGE_TYPE_2D;
        ici.format = fmt;
        ici.extent.width = width;
        ici.extent.height = height;
        ici.extent.depth = 1;
        ici.mipLevels = 1;
        ici.arrayLayers = 1;
        ici.samples = VK_SAMPLE_COUNT_1_BIT;
        ici.tiling = VK_IMAGE_TILING_OPTIMAL;
        ici.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        ici.flags = 0;
        if (vkCreateImage(device, &ici, nullptr, &image) != VK_SUCCESS) throw std::runtime_error("createImageSinglePlane: vkCreateImage failed");
        VkMemoryRequirements mr;
        vkGetImageMemoryRequirements(device, image, &mr);
        VkMemoryAllocateInfo mai{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        mai.allocationSize = mr.size;
        mai.memoryTypeIndex = findMemoryType(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (vkAllocateMemory(device, &mai, nullptr, &memory) != VK_SUCCESS) throw std::runtime_error("createImageSinglePlane: vkAllocateMemory failed");
        vkBindImageMemory(device, image, memory, 0);
        VkImageViewCreateInfo ivci{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        ivci.image = image;
        ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ivci.format = fmt;
        ivci.components = VkComponentMapping{ VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
        ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        ivci.subresourceRange.baseMipLevel = 0;
        ivci.subresourceRange.levelCount = 1;
        ivci.subresourceRange.baseArrayLayer = 0;
        ivci.subresourceRange.layerCount = 1;
        if (vkCreateImageView(device, &ivci, nullptr, &view) != VK_SUCCESS) throw std::runtime_error("createImageSinglePlane: vkCreateImageView failed");
}

void  VulkanRenderer:: createImageNV12Multi(uint32_t width, uint32_t height, VkImage& image, VkDeviceMemory& memory, VkImageView& view)
{
if (device == VK_NULL_HANDLE) return;
        VkFormat imgFmt = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM; // multi-planar format
        VkImageCreateInfo ici{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        ici.imageType = VK_IMAGE_TYPE_2D;
        ici.format = imgFmt;
        ici.extent.width = width;
        ici.extent.height = height;
        ici.extent.depth = 1;
        ici.mipLevels = 1;
        ici.arrayLayers = 1;
        ici.samples = VK_SAMPLE_COUNT_1_BIT;
        ici.tiling = VK_IMAGE_TILING_OPTIMAL;
        ici.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        ici.flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT; // 必须为 multi-planar + conversion 设置 mutable
        if (vkCreateImage(device, &ici, nullptr, &image) != VK_SUCCESS) throw std::runtime_error("createImageNV12: vkCreateImage failed");
        VkMemoryRequirements mr;
        vkGetImageMemoryRequirements(device, image, &mr);
        VkMemoryAllocateInfo mai{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        mai.allocationSize = mr.size;
        mai.memoryTypeIndex = findMemoryType(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (vkAllocateMemory(device, &mai, nullptr, &memory) != VK_SUCCESS) throw std::runtime_error("createImageNV12: vkAllocateMemory failed");
        vkBindImageMemory(device, image, memory, 0);

        // Create an imageView with the multi-planar format and attach the same conversion via pNext
        VkImageViewCreateInfo ivci{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        ivci.image = image;
        ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ivci.format = imgFmt; // MUST match conversion.format
        ivci.components = VkComponentMapping{ VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
        // For multi-planar view we use the color aspect (conversion will map planes)
        ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        ivci.subresourceRange.baseMipLevel = 0;
        ivci.subresourceRange.levelCount = 1;
        ivci.subresourceRange.baseArrayLayer = 0;
        ivci.subresourceRange.layerCount = 1;

        // Attach conversion info if we have one
        if (renderRes.ycbcrConversion != VK_NULL_HANDLE) {
            VkSamplerYcbcrConversionInfo convInfo{ VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO };
            convInfo.conversion = renderRes.ycbcrConversion;
            ivci.pNext = &convInfo;
            // convInfo is read during vkCreateImageView call — stack lifetime is OK here.
        }
        else {
            ivci.pNext = nullptr;
        }

        if (vkCreateImageView(device, &ivci, nullptr, &view) != VK_SUCCESS) throw std::runtime_error("createImageNV12: vkCreateImageView failed");
}

udword  VulkanRenderer:: curVideoWidth ()
{
    return m_curVideoWidth;
}

void  VulkanRenderer:: setCurVideoWidth (udword v)
{
    m_curVideoWidth = v;
}

udword  VulkanRenderer:: curVideoHeight ()
{
    return m_curVideoHeight;
}

void  VulkanRenderer:: setCurVideoHeight (udword v)
{
    m_curVideoHeight = v;
}
/* 计算保持宽高比的渲染区域（返回值：x, y, width, height）    */
struct RenderRect {
    float x;
    float y;
    float width;
    float height;
};

RenderRect calculateAspectRatioRect(udword windowWidth, udword windowHeight, udword videoWidth, udword videoHeight) {
    RenderRect rect{};

    /* 1. 计算窗口和视频的宽高比 */
    float windowAspect = static_cast<float>(windowWidth) / windowHeight;
    float videoAspect = static_cast<float>(videoWidth) / videoHeight;

    /* 2. 比较宽高比，确定缩放方式 */
    if (windowAspect > videoAspect) {
        /* 窗口更宽 → 视频高度填满窗口，宽度居中（左右黑边） */
        rect.height = static_cast<float>(windowHeight);
        rect.width = rect.height * videoAspect;
        rect.x = (static_cast<float>(windowWidth) - rect.width) / 2.0f;
        rect.y = 0.0f;
    } else {
        /* 窗口更高 → 视频宽度填满窗口，高度居中（上下黑边） */
        rect.width = static_cast<float>(windowWidth);
        rect.height = rect.width / videoAspect;
        rect.x = 0.0f;
        rect.y = (static_cast<float>(windowHeight) - rect.height) / 2.0f;
    }

    return rect;
}

void  VulkanRenderer:: createRenderResources()
{
    VkResult r = VK_SUCCESS;
        // RenderPass
        VkAttachmentDescription colorAtt{};
        colorAtt.format = swapchainFormat;
        colorAtt.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAtt.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAtt.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAtt.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAtt.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        VkAttachmentReference colorRef{};
        colorRef.attachment = 0;
        colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorRef;
        VkSubpassDependency dep{};
        dep.srcSubpass = VK_SUBPASS_EXTERNAL;
        dep.dstSubpass = 0;
        dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.srcAccessMask = 0;
        dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        VkRenderPassCreateInfo rpci{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
        rpci.attachmentCount = 1;
        rpci.pAttachments = &colorAtt;
        rpci.subpassCount = 1;
        rpci.pSubpasses = &subpass;
        rpci.dependencyCount = 1;
        rpci.pDependencies = &dep;
        r = vkCreateRenderPass(device, &rpci, nullptr, &renderRes.renderPass);
        if (r != VK_SUCCESS) throw std::runtime_error("vkCreateRenderPass failed: " + std::to_string(r));

        // Framebuffers
        swapFramebuffers.resize(swapViews.size());
        for (size_t i = 0; i < swapViews.size(); ++i) {
            VkFramebufferCreateInfo fci{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
            fci.renderPass = renderRes.renderPass;
            fci.attachmentCount = 1;
            fci.pAttachments = &swapViews[i];
            fci.width = extent.width;
            fci.height = extent.height;
            fci.layers = 1;
            r = vkCreateFramebuffer(device, &fci, nullptr, &swapFramebuffers[i]);
            if (r != VK_SUCCESS) throw std::runtime_error("vkCreateFramebuffer failed: " + std::to_string(r));
        }

        // Compile vertex shader always
        std::vector<uint32_t> vert_spv = compileGLSLtoSPV(kVertexGLSL, shaderc_glsl_vertex_shader, "fullscreen.vert");

        std::vector<uint32_t> frag_spv;
        if (ycbcrFeatureEnabled ()) {
            frag_spv = compileGLSLtoSPV(kFragYcbcrGLSL, shaderc_glsl_fragment_shader, "ycbcr.frag");
        }
        else {
            frag_spv = compileGLSLtoSPV(kFragManualGLSL, shaderc_glsl_fragment_shader, "manual.frag");
        }

        VkShaderModuleCreateInfo smci{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
        smci.codeSize = vert_spv.size() * sizeof(uint32_t);
        smci.pCode = vert_spv.data();
        r = vkCreateShaderModule(device, &smci, nullptr, &renderRes.vertShader);
        if (r != VK_SUCCESS) throw std::runtime_error("vkCreateShaderModule(vert) failed: " + std::to_string(r));
        smci.codeSize = frag_spv.size() * sizeof(uint32_t);
        smci.pCode = frag_spv.data();
        r = vkCreateShaderModule(device, &smci, nullptr, &renderRes.fragShader);
        if (r != VK_SUCCESS) throw std::runtime_error("vkCreateShaderModule(frag) failed: " + std::to_string(r));

        // --- IMPORTANT: create VkSamplerYcbcrConversion (if available and feature enabled) AND create sampler BEFORE descriptor set layout ---
        renderRes.ycbcrConversion = VK_NULL_HANDLE;
        bool haveYcbcrExt = deviceExtensionSupported(physical, VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME);
        if (ycbcrFeatureEnabled() && haveYcbcrExt) {
            VkSamplerYcbcrConversionCreateInfo convInfo{ VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO };
            convInfo.format = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
            // Try BT.709 + NARROW by default; can switch if source metadata indicates 601/full etc.
            convInfo.ycbcrModel = VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_709;
            convInfo.ycbcrRange = VK_SAMPLER_YCBCR_RANGE_ITU_NARROW;
            convInfo.components = VkComponentMapping{ VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
            convInfo.xChromaOffset = VK_CHROMA_LOCATION_COSITED_EVEN;
            convInfo.yChromaOffset = VK_CHROMA_LOCATION_COSITED_EVEN;
            convInfo.chromaFilter = VK_FILTER_LINEAR;
            convInfo.forceExplicitReconstruction = VK_FALSE;

            auto pCreateConv = (PFN_vkCreateSamplerYcbcrConversion)vkGetDeviceProcAddr(device, "vkCreateSamplerYcbcrConversion");
            if (pCreateConv) {
                if (pCreateConv(device, &convInfo, nullptr, &renderRes.ycbcrConversion) != VK_SUCCESS) {
                    std::cerr << "Warning: vkCreateSamplerYcbcrConversion failed; renderRes.ycbcrConversion = VK_NULL_HANDLE\n";
                    renderRes.ycbcrConversion = VK_NULL_HANDLE;
                }
            }
        }

        // Create VkSampler. If we created a ycbcrConversion, attach it to sampler.pNext (sampler will contain conversion)
        VkSamplerCreateInfo sci{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
        sci.magFilter = VK_FILTER_LINEAR;
        sci.minFilter = VK_FILTER_LINEAR;
        sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sci.anisotropyEnable = VK_FALSE;
        sci.maxAnisotropy = 1.0f;
        sci.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        sci.unnormalizedCoordinates = VK_FALSE;
        sci.compareEnable = VK_FALSE;
        sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sci.pNext = nullptr;

        VkSamplerYcbcrConversionInfo samplerConvInfo{ VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO };
        if (renderRes.ycbcrConversion != VK_NULL_HANDLE) {
            samplerConvInfo.conversion = renderRes.ycbcrConversion;
            sci.pNext = &samplerConvInfo;
        }

        if (vkCreateSampler(device, &sci, nullptr, &renderRes.sampler) != VK_SUCCESS) throw std::runtime_error("vkCreateSampler failed");

        // Descriptor Set Layout: depends on path
        VkDescriptorSetLayoutCreateInfo dslci{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        if (renderRes.ycbcrConversion != VK_NULL_HANDLE) {
            // single binding 0, combined image sampler
            VkDescriptorSetLayoutBinding binding{};
            binding.binding = 0;
            binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            binding.descriptorCount = 1;
            binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            // 使用 immutable sampler（sampler 包含 conversion），满足 descriptor validity
            binding.pImmutableSamplers = &renderRes.sampler;
            dslci.bindingCount = 1;
            dslci.pBindings = &binding;
        }
        else {
            // manual: two bindings (0 = Y, 1 = UV) both combined image sampler, no immutable samplers
            VkDescriptorSetLayoutBinding bindings[2]{};
            bindings[0].binding = 0;
            bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            bindings[0].descriptorCount = 1;
            bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            bindings[0].pImmutableSamplers = nullptr;
            bindings[1].binding = 1;
            bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            bindings[1].descriptorCount = 1;
            bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            bindings[1].pImmutableSamplers = nullptr;
            dslci.bindingCount = 2;
            dslci.pBindings = bindings;
        }
        r = vkCreateDescriptorSetLayout(device, &dslci, nullptr, &renderRes.descSetLayout);
        if (r != VK_SUCCESS) throw std::runtime_error("vkCreateDescriptorSetLayout failed: " + std::to_string(r));

        // Pipeline Layout
        VkPipelineLayoutCreateInfo plci{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        plci.setLayoutCount = 1;
        plci.pSetLayouts = &renderRes.descSetLayout;
        r = vkCreatePipelineLayout(device, &plci, nullptr, &renderRes.pipelineLayout);
        if (r != VK_SUCCESS) throw std::runtime_error("vkCreatePipelineLayout failed: " + std::to_string(r));

        // Graphics pipeline (vertex/fragment modules already created)
        VkPipelineShaderStageCreateInfo stagesInfo[2]{};
        stagesInfo[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stagesInfo[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stagesInfo[0].module = renderRes.vertShader;
        stagesInfo[0].pName = "main";
        stagesInfo[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stagesInfo[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stagesInfo[1].module = renderRes.fragShader;
        stagesInfo[1].pName = "main";

        VkPipelineVertexInputStateCreateInfo vici{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
        vici.vertexBindingDescriptionCount = 0;
        vici.vertexAttributeDescriptionCount = 0;

        VkPipelineInputAssemblyStateCreateInfo iaci{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
        iaci.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        iaci.primitiveRestartEnable = VK_FALSE;

        RenderRect renderRect = calculateAspectRatioRect(extent.width, extent.height, m_curVideoWidth, m_curVideoHeight);
        VkViewport viewport{};
        viewport.x = renderRect.x;
        viewport.y = renderRect.y;
        viewport.width = renderRect.width;
        viewport.height = renderRect.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        VkRect2D scissor{};
        scissor.offset = {static_cast<int32_t>(renderRect.x), static_cast<int32_t>(renderRect.y)};
        scissor.extent = {
            static_cast<uint32_t>(renderRect.width),
            static_cast<uint32_t>(renderRect.height)
        };
        VkPipelineViewportStateCreateInfo vpci{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
        vpci.viewportCount = 1;
        vpci.pViewports = &viewport;
        vpci.scissorCount = 1;
        vpci.pScissors = &scissor;

        VkPipelineRasterizationStateCreateInfo rsci{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
        rsci.depthClampEnable = VK_FALSE;
        rsci.rasterizerDiscardEnable = VK_FALSE;
        rsci.polygonMode = VK_POLYGON_MODE_FILL;
        rsci.cullMode = VK_CULL_MODE_BACK_BIT;
        rsci.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rsci.depthBiasEnable = VK_FALSE;
        rsci.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo msci{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
        msci.sampleShadingEnable = VK_FALSE;
        msci.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState cbas{};
        cbas.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        cbas.blendEnable = VK_FALSE;
        VkPipelineColorBlendStateCreateInfo cbsi{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
        cbsi.attachmentCount = 1;
        cbsi.pAttachments = &cbas;

        VkGraphicsPipelineCreateInfo gpci{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
        gpci.stageCount = 2;
        gpci.pStages = stagesInfo;
        gpci.pVertexInputState = &vici;
        gpci.pInputAssemblyState = &iaci;
        gpci.pViewportState = &vpci;
        gpci.pRasterizationState = &rsci;
        gpci.pMultisampleState = &msci;
        gpci.pColorBlendState = &cbsi;
        gpci.layout = renderRes.pipelineLayout;
        gpci.renderPass = renderRes.renderPass;
        gpci.subpass = 0;
        r = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &gpci, nullptr, &renderRes.pipeline);
        if (r != VK_SUCCESS) throw std::runtime_error("vkCreateGraphicsPipelines failed: " + std::to_string(r));

        // Per-frame resources
        renderRes.frames.resize(swapImages.size());
        // descriptor pool: choose descriptor count according to layout (1 or 2 descriptors per set)
        uint32_t descriptorsPerSet = (renderRes.ycbcrConversion != VK_NULL_HANDLE) ? 1u : 2u;
        std::vector<VkDescriptorSetLayout> layouts(swapImages.size(), renderRes.descSetLayout);
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSize.descriptorCount = (uint32_t)layouts.size() * descriptorsPerSet;
        VkDescriptorPoolCreateInfo dpci{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        dpci.maxSets = (uint32_t)layouts.size();
        dpci.poolSizeCount = 1;
        dpci.pPoolSizes = &poolSize;
        r = vkCreateDescriptorPool(device, &dpci, nullptr, &renderRes.descPool);
        if (r != VK_SUCCESS) throw std::runtime_error("vkCreateDescriptorPool failed: " + std::to_string(r));

        for (auto& frame : renderRes.frames) {
            VkCommandBufferAllocateInfo cbai{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
            cbai.commandPool = cmdPool;
            cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            cbai.commandBufferCount = 1;
            r = vkAllocateCommandBuffers(device, &cbai, &frame.cmdBuf);
            if (r != VK_SUCCESS) throw std::runtime_error("vkAllocateCommandBuffers failed: " + std::to_string(r));

            VkFenceCreateInfo fci{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
            fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
            r = vkCreateFence(device, &fci, nullptr, &frame.fence);
            if (r != VK_SUCCESS) throw std::runtime_error("vkCreateFence failed: " + std::to_string(r));

            VkSemaphoreCreateInfo sci2{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
            r = vkCreateSemaphore(device, &sci2, nullptr, &frame.imageAvailable);
            if (r != VK_SUCCESS) throw std::runtime_error("vkCreateSemaphore(imageAvailable) failed: " + std::to_string(r));
            r = vkCreateSemaphore(device, &sci2, nullptr, &frame.renderFinished);
            if (r != VK_SUCCESS) throw std::runtime_error("vkCreateSemaphore(renderFinished) failed: " + std::to_string(r));

            VkDescriptorSetAllocateInfo dsai{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
            dsai.descriptorPool = renderRes.descPool;
            dsai.descriptorSetCount = 1;
            VkDescriptorSetLayout layout = renderRes.descSetLayout;
            dsai.pSetLayouts = &layout;
            r = vkAllocateDescriptorSets(device, &dsai, &frame.descSet);
            if (r != VK_SUCCESS) throw std::runtime_error("vkAllocateDescriptorSets failed: " + std::to_string(r));
        }
}

void  VulkanRenderer:: cleanupRenderResources()
{
    for (auto& frame : renderRes.frames) {
        if (frame.imageView) vkDestroyImageView(device, frame.imageView, nullptr);
        if (frame.image) vkDestroyImage(device, frame.image, nullptr);
        if (frame.imageMem) vkFreeMemory(device, frame.imageMem, nullptr);

        if (frame.imageViewY) vkDestroyImageView(device, frame.imageViewY, nullptr);
        if (frame.imageY) vkDestroyImage(device, frame.imageY, nullptr);
        if (frame.imageYMem) vkFreeMemory(device, frame.imageYMem, nullptr);

        if (frame.imageViewUV) vkDestroyImageView(device, frame.imageViewUV, nullptr);
        if (frame.imageUV) vkDestroyImage(device, frame.imageUV, nullptr);
        if (frame.imageUVMem) vkFreeMemory(device, frame.imageUVMem, nullptr);

        if (frame.fence) vkDestroyFence(device, frame.fence, nullptr);
        if (frame.imageAvailable) vkDestroySemaphore(device, frame.imageAvailable, nullptr);
        if (frame.renderFinished) vkDestroySemaphore(device, frame.renderFinished, nullptr);
        if (frame.cmdBuf != VK_NULL_HANDLE) {
            vkFreeCommandBuffers(device, cmdPool, 1, &frame.cmdBuf);
            frame.cmdBuf = VK_NULL_HANDLE;
        }
    }
    renderRes.frames.clear();
    if (renderRes.sampler) vkDestroySampler(device, renderRes.sampler, nullptr);
    if (renderRes.ycbcrConversion) {
        auto pDestroy = (PFN_vkDestroySamplerYcbcrConversion)vkGetDeviceProcAddr(device, "vkDestroySamplerYcbcrConversion");
        if (pDestroy) pDestroy(device, renderRes.ycbcrConversion, nullptr);
        else if (vkDestroySamplerYcbcrConversion != nullptr) vkDestroySamplerYcbcrConversion(device, renderRes.ycbcrConversion, nullptr);
    }
    if (renderRes.descPool) vkDestroyDescriptorPool(device, renderRes.descPool, nullptr);
    if (renderRes.pipeline) vkDestroyPipeline(device, renderRes.pipeline, nullptr);
    if (renderRes.pipelineLayout) vkDestroyPipelineLayout(device, renderRes.pipelineLayout, nullptr);
    if (renderRes.descSetLayout) vkDestroyDescriptorSetLayout(device, renderRes.descSetLayout, nullptr);
    if (renderRes.fragShader) vkDestroyShaderModule(device, renderRes.fragShader, nullptr);
    if (renderRes.vertShader) vkDestroyShaderModule(device, renderRes.vertShader, nullptr);
    if (renderRes.renderPass) vkDestroyRenderPass(device, renderRes.renderPass, nullptr);
}

void  VulkanRenderer:: recreateSwapchain()
{
    vkDeviceWaitIdle(device);
    cleanupRenderResources();
    for (auto fb : swapFramebuffers) vkDestroyFramebuffer(device, fb, nullptr);
    swapFramebuffers.clear();
    for (auto view : swapViews) vkDestroyImageView(device, view, nullptr);
    swapViews.clear();
    if (swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, swapchain, nullptr);
        swapchain = VK_NULL_HANDLE;
    }
    createSwapchain();
    createRenderResources();
}

VkCommandBuffer  VulkanRenderer:: beginSingleTimeCommands()
{
    VkCommandBufferAllocateInfo alloc{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    alloc.commandPool = cmdPool;
    alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc.commandBufferCount = 1;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    vkAllocateCommandBuffers(device, &alloc, &cmd);
    VkCommandBufferBeginInfo bi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &bi);
    return cmd;
}

void  VulkanRenderer:: endSingleTimeCommands(VkCommandBuffer cmd)
{
        vkEndCommandBuffer(cmd);
        VkSubmitInfo si{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
        si.commandBufferCount = 1;
        si.pCommandBuffers = &cmd;
        vkQueueSubmit(m_graphicsQueue, 1, &si, VK_NULL_HANDLE);
        vkQueueWaitIdle(m_graphicsQueue);
        vkFreeCommandBuffers(device, cmdPool, 1, &cmd);
}

void  VulkanRenderer:: createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkBuffer& buf, VkDeviceMemory& mem)
{
if (device == VK_NULL_HANDLE) return;
        VkBufferCreateInfo bci{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        bci.size = size;
        bci.usage = usage;
        bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vkCreateBuffer(device, &bci, nullptr, &buf);
        VkMemoryRequirements mr;
        vkGetBufferMemoryRequirements(device, buf, &mr);
        VkMemoryAllocateInfo mai{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        mai.allocationSize = mr.size;
        mai.memoryTypeIndex = findMemoryType(mr.memoryTypeBits, props);
        vkAllocateMemory(device, &mai, nullptr, &mem);
        vkBindBufferMemory(device, buf, mem, 0);
}

void  VulkanRenderer:: transitionImageLayout(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageAspectFlags aspectMask)
{
    VkImageMemoryBarrier barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = aspectMask;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    VkPipelineStageFlags srcStage, dstStage;
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else {
        return;
    }
    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

uint32_t  VulkanRenderer:: findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags props)
{
VkPhysicalDeviceMemoryProperties memProps;
        vkGetPhysicalDeviceMemoryProperties(physical, &memProps);
        for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
            if ((typeFilter & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props) return i;
        }
        throw std::runtime_error("findMemoryType failed");
}

bool  VulkanRenderer:: ycbcrFeatureEnabled ()
{
    return m_ycbcrFeatureEnabled;
}

void  VulkanRenderer:: setYcbcrFeatureEnabled (bool v)
{
    m_ycbcrFeatureEnabled = v;
}
int   VulkanRenderer:: imageCount ()
{
    return m_imageCount;
}

int   VulkanRenderer:: minImageCount ()
{
    return m_minImageCount;
}

VkInstance  VulkanRenderer:: getInstance ()
{
    return instance;
}

VkPhysicalDevice  VulkanRenderer:: physicalDevice ()
{
    return physical;
}

VkDevice  VulkanRenderer:: getDevice ()
{
    return device;
}

int   VulkanRenderer:: graphicsFamily ()
{
    return m_graphicsQueueFamily;
}

VkQueue  VulkanRenderer:: graphicsQueue()
{
    return m_graphicsQueue;
}

VkRenderPass  VulkanRenderer:: getRenderPass()
{
    return renderRes.renderPass;
}

SDL_Window*   VulkanRenderer:: getSDLWindow()
{
    return window;
}

int   VulkanRenderer:: addDrawCallback (drawCallback* pI)
{
    int   nRet = 0;
    do {
        m_drawCallbacks.push_back(pI);
    } while (0);
    return nRet;
}
VulkanRenderer:: VulkanRenderer ()
{
}

VulkanRenderer:: ~VulkanRenderer ()
{
}

