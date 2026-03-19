#include "Core/Platform/AppWindow.h"
#include "Core/Utils/Log.h"
#include "Core/Graphics/VulkanRenderer.h"
#include "AppLogic.h"
#include "Core/Editor/UIManager.h"
#include "Core/GameSystem/Components/CCamera.h"
#include "Core/GameSystem/Components/CTransform.h"
#include"imgui.h"

int main() {
    Log::Get().Init();
    LOG_INFO("Tumbler Engine Starting...");

    try {
        AppWindow::AppWindowConfig config;
        config.Title = "Tumbler Engine - Scene Rendering";
        AppWindow window(config);

        VulkanRenderer renderer;
        renderer.Init(&window);

        // 1. 实例化我们的应用逻辑 (场景会在这里被搭建)
        AppLogic logic;
        logic.InitializeMaterials(&renderer);

        // 【优化】：提前上传共用网格，防止渲染中途卡顿
        renderer.UploadMesh(logic.GetDefaultMesh().get());

        // 2. 创建一个虚拟相机
        CTransform cameraTransform;
        CCamera cameraComponent;

        // 【位置】放在靠近正面开口处 (Z=4.0)，高度略低于正中心 (Y=-1.0) 更有空间透视感
        cameraTransform.SetPosition(glm::vec3(0.0f, -1.0f, 16.0f));

        // 【旋转】因为 FQuaternion::GetForwardVector 默认是 +Z (0,0,1)
        // 我们的 BackWall 在 -Z (-5)，所以要绕 Y 轴 (Yaw) 旋转 180 度回头看
        float cameraYaw = 180.0f;
        float cameraPitch = 0.0f;
        cameraTransform.SetRotation(glm::vec3(cameraPitch, cameraYaw, 0.0f));

        // 【视野】设为 60 度或 70 度，能把整个盒子的 5 面墙都框进屏幕
        cameraComponent.Fov = 60.0f;
        auto startTime = std::chrono::high_resolution_clock::now();

        UIManager ui_manager;
        ui_manager.Init(&window, &renderer);

        while (!window.ShouldClose()) {
            window.PollEvents();

            ui_manager.BeginFrame();
            ImGui::Begin("PBR Debug Engine");
            ImGui::DragFloat3("Light Pos", &renderer.GlobalLightPos.x, 0.1f, -10.0f, 10.0f);
            ImGui::ColorEdit3("Light Color", &renderer.GlobalLightColor.x);
            ImGui::SliderFloat("Light Power", &renderer.GlobalLightIntensity, 0.0f, 200.0f);
            ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
            ImGui::End();
            ui_manager.EndFrame();

            // 3. 将场景和相机提交给渲染器
            renderer.Render(logic.GetScene(), &cameraComponent, &cameraTransform, [&](VkCommandBuffer cmdBuffer){
                ui_manager.RecordDrawCommands(cmdBuffer);
            });
        }

        vkDeviceWaitIdle(renderer.GetDevice());

        // 2. 清理门户：手动销毁 ImGui 底层庞大的 Vulkan 资源
        ui_manager.Cleanup(renderer.GetDevice());

    } catch (const std::exception& e) {
        LOG_CRITICAL("Crash: {}", e.what());
        return -1;
    }

    return 0;
}
