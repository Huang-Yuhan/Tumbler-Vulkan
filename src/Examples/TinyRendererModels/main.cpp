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
    LOG_INFO("Tumbler Engine - Tiny Renderer Models Starting...");

    try {
        AppWindow::AppWindowConfig config;
        config.Title = "Tumbler Engine - Tiny Renderer Models";
        AppWindow window(config);

        VulkanRenderer renderer;
        renderer.Init(&window);

        FAssetManager assetManager;
        assetManager.Initialize(&renderer);

        InputManager inputManager;
        inputManager.Init(window.GetNativeWindow());

        inputManager.BindAxis("MoveForward", EKeyCode::W, EKeyCode::S);
        inputManager.BindAxis("MoveRight", EKeyCode::D, EKeyCode::A);
        inputManager.BindAxis("MoveUp", EKeyCode::E, EKeyCode::Q);

        AppLogic logic;
        logic.Init(&renderer, &assetManager, &inputManager);

        auto currentTime = std::chrono::high_resolution_clock::now();

        UIManager ui_manager;
        ui_manager.Init(&window, &renderer);

        while (!window.ShouldClose()) {
            window.PollEvents();

            auto newTime = std::chrono::high_resolution_clock::now();
            float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
            currentTime = newTime;

            if (window.IsFramebufferResized()) {
                window.ClearResizedFlag();
                LOG_INFO("Window resized, recreating swapchain...");
            }

            inputManager.Tick();
            logic.Tick(frameTime);

            ui_manager.BeginFrame();
            ImGui::Begin("Tiny Renderer Models Debug");

            if (FActor* mainLight = logic.GetScene()->FindActorByName("MainLight")) {
                if (auto* pl = mainLight->GetComponent<CPointLight>()) {
                    glm::vec3 pos = mainLight->Transform.GetPosition();
                    if (ImGui::DragFloat3("Light Pos", &pos.x, 0.1f, -20.0f, 20.0f)) {
                        mainLight->Transform.SetPosition(pos);
                    }
                    ImGui::ColorEdit3("Light Color", &pl->Color.x);
                    ImGui::SliderFloat("Light Power", &pl->Intensity, 0.0f, 500.0f);
                }
            } else {
                ImGui::Text("MainLight not found");
            }

            ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
            ImGui::End();
            ui_manager.EndFrame();

            std::vector<RenderPacket> renderPackets;
            logic.GetScene()->ExtractRenderPackets(renderPackets);

            VkExtent2D swapchainExtent = renderer.GetSwapchainExtent();
            float aspectRatio = swapchainExtent.height == 0
                ? 1.0f
                : static_cast<float>(swapchainExtent.width) / static_cast<float>(swapchainExtent.height);
            
            CFirstPersonCamera* cam = logic.GetMainCamera();
            SceneViewData viewData = logic.GetScene()->GenerateSceneView(cam, &cam->GetOwner()->Transform, aspectRatio);

            renderer.Render(viewData, renderPackets, [&](VkCommandBuffer cmd) {
                ui_manager.RecordDrawCommands(cmd);
            });
        }

        vkDeviceWaitIdle(renderer.GetDevice());
        ui_manager.Cleanup(renderer.GetDevice());

    } catch (const std::exception& e) {
        LOG_CRITICAL("Crash: {}", e.what());
        return -1;
    }

    return 0;
}
