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

#include "Core/GameSystem/FActor.h"

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

        AppLogic logic;
        logic.Init(&renderer, &assetManager);

        // 提前上传共用网格，防止渲染中途卡顿
        renderer.UploadMesh(assetManager.GetOrLoadMesh("DefaultPlane").get());

        // 3. 创建虚拟相机 (属于游戏逻辑世界)
        CTransform cameraTransform;
        CCamera cameraComponent;
        cameraTransform.SetPosition(glm::vec3(0.0f, -1.0f, 16.0f));
        cameraTransform.SetRotation(glm::vec3(0.0f, 180.0f, 0.0f));
        cameraComponent.Fov = 60.0f;

        // 4. UI 系统初始化
        UIManager ui_manager;
        ui_manager.Init(&window, &renderer);

        // ==========================================
        // 核心游戏与渲染主循环
        // ==========================================
        while (!window.ShouldClose()) {
            window.PollEvents();

            // --- A. 游戏 UI 与交互逻辑 ---
            ui_manager.BeginFrame();
            ImGui::Begin("PBR Debug Engine");

            if (FActor* mainLight = logic.GetScene()->FindActorByName("MainLight")) {
                if (auto* pl = mainLight->GetComponent<CPointLight>()) {
                    // 控制光源的 Transform
                    glm::vec3 pos = mainLight->Transform.GetPosition();
                    if (ImGui::DragFloat3("Light Pos", &pos.x, 0.1f, -10.0f, 10.0f)) {
                        mainLight->Transform.SetPosition(pos);
                    }
                    // 控制光源的组件属性
                    ImGui::ColorEdit3("Light Color", &pl->Color.x);
                    ImGui::SliderFloat("Light Power", &pl->Intensity, 0.0f, 200.0f);
                }
            } else {
                ImGui::Text("MainLight not found");
            }

            ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
            ImGui::End();
            ui_manager.EndFrame();

            // --- B. 数据打包装车 ---

            // 1. 提取客观物体数据 (RenderPackets)
            std::vector<RenderPacket> renderPackets;
            logic.GetScene()->ExtractRenderPackets(renderPackets);

            // 2. 提取当前观察者的视图与环境数据 (SceneViewData)
            SceneViewData viewData = logic.GetScene()->GenerateSceneView(&cameraComponent, &cameraTransform);

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
