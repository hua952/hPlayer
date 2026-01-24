#ifndef _VulkanRenderer_h__
#define _VulkanRenderer_h__
#include <memory>
#include <vector>
#include <string>
#include <vulkan/vulkan.h>
#include <shaderc/shaderc.hpp> // runtime GLSL compiler
#include <SDL.h>
#include <SDL_vulkan.h>

#include "comman.h"
#include "comFun.h"

struct  videoFrameData;
// struct presentNV12Ask;
// === Vulkan Rendering Resources ===
struct VulkanRenderResources {
    VkDescriptorSetLayout descSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkShaderModule vertShader = VK_NULL_HANDLE;
    VkShaderModule fragShader = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE; // used for manual path OR sampler with ycbcr conversion
    VkDescriptorPool descPool = VK_NULL_HANDLE;
    VkSamplerYcbcrConversion ycbcrConversion = VK_NULL_HANDLE;

    struct Frame {
        // Multi-planar image (used when ycbcr conversion path)
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory imageMem = VK_NULL_HANDLE;
        VkImageView imageView = VK_NULL_HANDLE;

        // Single-plane images (manual fallback)
        VkImage imageY = VK_NULL_HANDLE;
        VkDeviceMemory imageYMem = VK_NULL_HANDLE;
        VkImageView imageViewY = VK_NULL_HANDLE;

        VkImage imageUV = VK_NULL_HANDLE;
        VkDeviceMemory imageUVMem = VK_NULL_HANDLE;
        VkImageView imageViewUV = VK_NULL_HANDLE;

        VkDescriptorSet descSet = VK_NULL_HANDLE;
        VkCommandBuffer cmdBuf = VK_NULL_HANDLE;
        VkFence fence = VK_NULL_HANDLE;
        VkSemaphore imageAvailable = VK_NULL_HANDLE;
        VkSemaphore renderFinished = VK_NULL_HANDLE;
        VkExtent2D imageExtent{ 0, 0 };
        bool inFlight = false;
    };
    std::vector<Frame> frames;
};



class VulkanRenderer
{
public:
    enum renderState {
        renderState_OK,
        renderState_noSwapchain
    };
    VulkanRenderer ();
    ~VulkanRenderer ();
    void   init(SDL_Window* win);
    int  addDrawCallback (drawCallback* pI);
    void cleanup();
    bool presentNV12(const videoFrameData& vf);

    bool  ycbcrFeatureEnabled ();
    void  setYcbcrFeatureEnabled (bool v);

    int  imageCount ();
    int  minImageCount ();
    VkInstance getInstance ();
    VkPhysicalDevice physicalDevice ();
    VkDevice getDevice ();
    int  graphicsFamily ();
    VkQueue graphicsQueue();
    VkRenderPass getRenderPass();
    SDL_Window*  getSDLWindow();
    renderState  state();
    void         setState(renderState st);
    udword  curVideoWidth ();
    void  setCurVideoWidth (udword v);
    udword  curVideoHeight ();
    void  setCurVideoHeight (udword v);
private:
    udword  m_curVideoHeight;
    udword  m_curVideoWidth;
    renderState  m_renderState = renderState_OK;
    SDL_Window* window = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;
    uint32_t m_graphicsQueueFamily = UINT32_MAX;
    uint32_t presentQueueFamily = UINT32_MAX;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFormat swapchainFormat = VK_FORMAT_B8G8R8A8_UNORM;
    VkExtent2D extent{};
    std::vector<VkImage> swapImages;
    std::vector<VkImageView> swapViews;
    std::vector<VkFramebuffer> swapFramebuffers;

    std::vector<drawCallback*> m_drawCallbacks;

    VkCommandPool cmdPool = VK_NULL_HANDLE;

    int  m_imageCount = 0;
    int  m_minImageCount = 0;
    bool enableValidation = true;
    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
    bool  m_ycbcrFeatureEnabled = false;
    // whether device+driver feature samplerYcbcrConversion was enabled during device create
    VulkanRenderResources renderRes;

    VkResult createDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pMessenger);
    void destroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT messenger, const VkAllocationCallbacks* pAllocator);
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& info);
    void setupDebugMessenger();
    bool checkLayerAvailable(const char* layerName);
    void createSurface();
    void createInstance();
    void pickPhysicalDevice();
    bool isDeviceSuitable(VkPhysicalDevice d);
    bool deviceExtensionSupported(VkPhysicalDevice dev, const char* extName);
    void createDeviceAndQueues();
    void createSwapchain();
    void createCommandPool();
    std::vector<uint32_t> compileGLSLtoSPV(const std::string& source, shaderc_shader_kind kind, const char* source_name);
    void createImageSinglePlane(uint32_t width, uint32_t height, VkFormat fmt, VkImage& image, VkDeviceMemory& memory, VkImageView& view);
    void createImageNV12Multi(uint32_t width, uint32_t height, VkImage& image, VkDeviceMemory& memory, VkImageView& view);
    void createRenderResources();
    void cleanupRenderResources();
    void recreateSwapchain();
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer cmd);
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkBuffer& buf, VkDeviceMemory& mem);
    void transitionImageLayout(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags props);
};
#endif
