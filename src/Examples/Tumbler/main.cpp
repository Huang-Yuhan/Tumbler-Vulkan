#include "Core/Platform/AppWindow.h"
#include "Core/Utils/Log.h"
#include "Core/Graphics/VulkanRenderer.h"
#include "AppLogic.h"
#include "Core/GameSystem/Components/CCamera.h"
#include "Core/GameSystem/Components/CTransform.h"

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

        while (!window.ShouldClose()) {
            window.PollEvents();

            auto currentTime = std::chrono::high_resolution_clock::now();
            float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

            // （可选）让相机绕着场景慢慢转圈，更有立体感
            //cameraTransform.SetPosition(glm::vec3(sin(time * 0.5f) * 10.0f, 2.0f, cos(time * 0.5f) * 10.0f));
            // 简单的 LookAt 计算，让相机永远盯着原点
            glm::mat4 lookAt = glm::lookAt(cameraTransform.GetPosition(), glm::vec3(0,0,0), glm::vec3(0,1,0));
            // 这里我们用点数学 trick 把 LookAt 矩阵转回欧拉角，你也可以直接用 CCamera 的原生支持
            // 为了简单，我们这次允许略微的视线偏移，先看效果

            // 3. 将场景和相机提交给渲染器
            renderer.Render(logic.GetScene(), &cameraComponent, &cameraTransform);
        }

    } catch (const std::exception& e) {
        LOG_CRITICAL("Crash: {}", e.what());
        return -1;
    }

    return 0;
}
