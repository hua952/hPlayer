#ifndef _imGuiMgr_h__
#define _imGuiMgr_h__
#include <memory>
#include "comman.h"

class VulkanRenderer;
class ffmpegDecoder;
class imGuiMgr:public drawCallback
{
public:
    imGuiMgr (VulkanRenderer& rVulkanRenderer);
    ~imGuiMgr ();
    int onInit() override;
    void onDraw(VkCommandBuffer commandBuffer) override;
    void cleanup() override;
    double  currentPlayPos ();
    void setCurrentPlayPos (double pos);
    double  totalDuration ();
    void   setTotalDuration (double dur);
private:
    double  m_totalDuration;
    double  m_currentPlayPos;
    void renderImGuiControls();
    VulkanRenderer& m_VulkanRenderer;
    VkDescriptorPool m_imguiDescriptorPool;
};
#endif
