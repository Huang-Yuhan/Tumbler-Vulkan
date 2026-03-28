#include "Core/Platform/AppWindow.h"
#include "Core/Utils/Log.h"
#include "Core/Graphics/VulkanRenderer.h"
#include "AppLogic.h"
#include "Core/Assets/FAssetManager.h"
#include "Core/Editor/UIManager.h"
#include "Core/GameSystem/Components/CCamera.h"
#include "Core/GameSystem/Components/CTransform.h"
#include "Core/GameSystem/Components/CPointLight.h"
#include <imgui.h>
#include <vector>
#include <chrono>

#include "Core/GameSystem/FActor.h"
#include "Core/GameSystem/InputManager.h"
#include "Core/GameSystem/Components/CFirstPersonCamera.h"

int main() {
    Log::Get().Init();
    LOG_INFO("Tumbler Engine Starting...");

    try {
        // 1. 基础系统初始化
        AppWindow::AppWindowConfig config;
        config.Title = "Tumbler Engine - PBR Architecture";
        AppWindow window(config);

        VulkanRenderer renderer;
        renderer.Init(&window);

        // 2. 资源管理与场景级逻辑
        FAssetManager assetManager;
        assetManager.Initialize(&renderer);

        InputManager inputManager;
        inputManager.Init(window.GetNativeWindow());

        // 绑定输入
        inputManager.BindAxis("MoveForward", EKeyCode::W, EKeyCode::S);
        inputManager.BindAxis("MoveRight", EKeyCode::D, EKeyCode::A);
        inputManager.BindAxis("MoveUp", EKeyCode::E, EKeyCode::Q);

        AppLogic logic;
        logic.Init(&renderer, &assetManager, &inputManager);

        // 提前上传共用网格，防止渲染中途卡顿
        renderer.UploadMesh(assetManager.GetOrLoadMesh("DefaultPlane").get());

        // 3. 准备真实 DeltaTime 计时器
        auto currentTime = std::chrono::high_resolution_clock::now();

        // 4. UI 系统初始化
        UIManager ui_manager;
        ui_manager.Init(&window, &renderer);

        // ==========================================
        // 核心游戏与渲染主循环
        // ==========================================
        while (!window.ShouldClose()) {
            window.PollEvents();

            auto newTime = std::chrono::high_resolution_clock::now();
            float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
            currentTime = newTime;

            // 检查是否需要重建 Swapchain（窗口大小改变）
            if (window.IsFramebufferResized()) {
                window.ClearResizedFlag();
                LOG_INFO("Window resized, recreating swapchain...");
            }

            // 更新输入系统
            inputManager.Tick();

            // 更新游戏逻辑 (相机漫游等)
            logic.Tick(frameTime);

            // --- B. 数据打包装车 (先提取数据，用于性能统计) ---

            // 1. 提取客观物体数据 (RenderPackets)
            std::vector<RenderPacket> renderPackets;
            logic.GetScene()->ExtractRenderPackets(renderPackets);

            // --- A. 游戏 UI 与交互逻辑 ---
            ui_manager.BeginFrame();
            
            logic.UpdatePerformanceStats(frameTime, static_cast<int>(renderPackets.size()));
            logic.DrawEditorUI();
            
            ui_manager.EndFrame();

            // 2. 提取当前观察者的视图与环境数据 (SceneViewData)
            VkExtent2D swapchainExtent = renderer.GetSwapchainExtent();
            float aspectRatio = swapchainExtent.height == 0
                ? 1.0f
                : static_cast<float>(swapchainExtent.width) / static_cast<float>(swapchainExtent.height);
            
            CFirstPersonCamera* cam = logic.GetMainCamera();
            SceneViewData viewData = logic.GetScene()->GenerateSceneView(cam, &cam->GetOwner()->Transform, aspectRatio);
            
            // 将从 UI 获取的管线枚举注入
            viewData.RenderPath = logic.GetCurrentRenderPath();

            // --- C. 发送给底层渲染器执行 ---
            // 渲染器同时接收“视图”和“包裹”，并将 UI 录制指令作为回调传入
            renderer.Render(viewData, renderPackets, [&](VkCommandBuffer cmd) {
                ui_manager.RecordDrawCommands(cmd);
            });
        }

        // ==========================================
        // 安全退出与资源清理
        // ==========================================
        vkDeviceWaitIdle(renderer.GetDevice());
        ui_manager.Cleanup(renderer.GetDevice());

    } catch (const std::exception& e) {
        LOG_CRITICAL("Crash: {}", e.what());
        return -1;
    }

    return 0;
}
