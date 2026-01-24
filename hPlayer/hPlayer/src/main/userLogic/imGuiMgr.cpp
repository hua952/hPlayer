#include "imGuiMgr.h"
#include "VulkanRenderer.h"
#include "ffmpegDecoder.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"
#include "gLog.h"

double mainClockSec ();
static void check_vk_result(VkResult err)
{
    if (err == 0)
        return;
    fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
    if (err < 0)
        abort();
}

imGuiMgr:: imGuiMgr (VulkanRenderer& rVulkanRenderer):m_VulkanRenderer(rVulkanRenderer)
{
}

imGuiMgr:: ~imGuiMgr ()
{
}

int  imGuiMgr::onInit()
{
    int   nRet = 0;
    do {
        // === 初始化 ImGui ===
        auto& rRender = m_VulkanRenderer;
        // Descriptor Pool for ImGui
        VkDescriptorPoolSize pool_sizes[] = {
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1},
        };
        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 1;
        pool_info.poolSizeCount = 1;
        pool_info.pPoolSizes = pool_sizes;
        vkCreateDescriptorPool(rRender.getDevice(), &pool_info, nullptr, &m_imguiDescriptorPool);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        /*
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForVulkan(rRender.getGLFWwindow (), true);
        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = rRender.getInstance();
        init_info.PhysicalDevice = rRender.physicalDevice();
        init_info.Device = rRender.getDevice();
        init_info.QueueFamily = rRender.graphicsFamily();
        init_info.Queue = rRender.graphicsQueue ();
        init_info.PipelineCache = VK_NULL_HANDLE;
        init_info.DescriptorPool = m_imguiDescriptorPool;
        init_info.MinImageCount = rRender.minImageCount ();
        init_info.ImageCount = rRender.imageCount();
        init_info.UseDynamicRendering = false;
        init_info.PipelineInfoMain.RenderPass = rRender.getRenderPass();
        init_info.PipelineInfoMain.Subpass = 0;
        init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        ImGui_ImplVulkan_Init(&init_info); // ✅ renderPass here
        */
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
        // io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // IF using Docking Branch

        // Setup Platform/Renderer backends
        ImGui_ImplSDL2_InitForVulkan(rRender.getSDLWindow());
        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = rRender.getInstance();
        init_info.PhysicalDevice = rRender.physicalDevice();
        init_info.Device = rRender.getDevice();
        init_info.QueueFamily = rRender.graphicsFamily();
        init_info.Queue = rRender.graphicsQueue ();
        init_info.PipelineCache = VK_NULL_HANDLE;
        init_info.DescriptorPool = m_imguiDescriptorPool;
        init_info.MinImageCount = rRender.minImageCount ();
        init_info.ImageCount = rRender.imageCount();
        // init_info.Allocator = YOUR_ALLOCATOR;
        init_info.PipelineInfoMain.RenderPass = rRender.getRenderPass();
        init_info.PipelineInfoMain.Subpass = 0;
        init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        init_info.CheckVkResultFn = check_vk_result;
        ImGui_ImplVulkan_Init(&init_info);
    } while (0);
    return nRet;
}

void  imGuiMgr:: onDraw(VkCommandBuffer commandBuffer)
{
    do {
        /*
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        {
            ImGui::Begin("Hello, Vulkan!");
            ImGui::Text("This is a Vulkan triangle with ImGui overlay.");
            if (ImGui::Button("Click me!")) {
                // Do something
            }
            ImGui::End();
        }
        ImGui::Render();
        ImDrawData* draw_data = ImGui::GetDrawData();
        ImGui_ImplVulkan_RenderDrawData(draw_data, commandBuffer);
        */
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        // ImGui::ShowDemoWindow();
        renderImGuiControls ();
        ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
    } while (0);
}

void  imGuiMgr:: cleanup()
{
    do {
        // 清理
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
        auto& rRender = m_VulkanRenderer;
        vkDestroyDescriptorPool(rRender.getDevice(), m_imguiDescriptorPool, nullptr);
    } while (0);
}

double   imGuiMgr:: currentPlayPos ()
{
    return m_currentPlayPos;
}

void  imGuiMgr:: setCurrentPlayPos (double pos)
{
    m_currentPlayPos = pos;
}

double   imGuiMgr:: totalDuration ()
{
    return m_totalDuration;
}

void imGuiMgr:: setTotalDuration (double dur)
{
    m_totalDuration = dur;
}

void imGuiMgr::renderImGuiControls()
{
    double currentTime = mainClockSec();//player.audioClockSec ();
    double totalDuration = this->totalDuration();

    // 防止除零 & 无效值
    if (totalDuration <= 0.0) {
        ImGui::Text("Loading...");
        return;
    }

    // 限制 currentTime 在 [0, totalDuration]
    currentTime = std::clamp(currentTime, 0.0, totalDuration);

    // 设置滑块宽度为窗口剩余宽度
    float sliderWidth = ImGui::GetContentRegionAvail().x;
    ImGui::SetNextItemWidth(sliderWidth);

    // 格式化显示 "1:23 / 4:56"
    char displayBuf[64];
    auto formatTime = [](double t) -> std::string {
        int totalSecs = static_cast<int>(t);
        int mins = totalSecs / 60;
        int secs = totalSecs % 60;
        return std::to_string(mins) + ":" + (secs < 10 ? "0" : "") + std::to_string(secs);
    };
    snprintf(displayBuf, sizeof(displayBuf), "%s / %s",
             formatTime(currentTime).c_str(),
             formatTime(totalDuration).c_str());

    // 使用 float 临时变量供 SliderFloat 使用
    float ct_f = static_cast<float>(currentTime);
    float td_f = static_cast<float>(totalDuration);

    if (ImGui::SliderFloat("##Progress", &ct_f, 0.0f, td_f, displayBuf)) {
        // 用户拖动：发起跳转
        // player.seekTo(static_cast<double>(ct_f));
        gInfo("UI seekto : "<<ct_f);
    }
}

