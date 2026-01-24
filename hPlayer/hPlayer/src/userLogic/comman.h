#ifndef _comman_h__
#define _comman_h__
#include <vector>
#include <vulkan/vulkan.h>
struct drawCallback
{
    virtual int onInit() = 0;
    virtual void onDraw(VkCommandBuffer commandBuffer) = 0;
    virtual void cleanup() = 0;
};

// VideoFrame contains NV12 planes
struct VideoFrame {
    int width;
    int height;
    double pts_seconds;
    // Y plane
    std::vector<uint8_t> y_plane;
    int y_linesize = 0;
    // UV plane (interleaved NV12)
    std::vector<uint8_t> uv_plane;
    int uv_linesize = 0;
};
#endif
