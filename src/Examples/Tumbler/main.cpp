#include "Core/Platform/AppWindow.h"
#include "Core/Utils/Log.h"
#include "Core/Graphics/VulkanRenderer.h"
#include "Core/Geometry/FMesh.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

int main() {
    Log::Get().Init();
    LOG_INFO("Tumbler Engine Starting...");

    try {
        AppWindow::AppWindowConfig config;
        config.Title = "Tumbler Engine - Plane Rendering";
        config.Width = 1280;
        config.Height = 720;
        AppWindow window(config);

        VulkanRenderer renderer;
        renderer.Init(&window);

        // 1. 创建 CPU 数据 (平面)
        FMesh planeMesh = FMesh::CreatePlane(1.0f, 1.0f, 1, 1);

        // 2. 上传到 GPU
        FVulkanMesh& gpuMesh = renderer.UploadMesh(&planeMesh);
        // 记录开始时间
        auto startTime = std::chrono::high_resolution_clock::now();

        while (!window.ShouldClose()) {
            window.PollEvents();

            // 1. 计算时间差
            auto currentTime = std::chrono::high_resolution_clock::now();
            float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

            // 2. 计算 MVP 矩阵 (Model-View-Projection)

            // Model: 绕 Y 轴旋转
            // glm::rotate(单位矩阵, 角度(弧度), 旋转轴)
            glm::mat4 model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));

            // View: 相机往后退一点 (Z = -2.5)
            // lookAt(相机位置, 目标位置, 上方向)
            glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 1.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

            // Projection: 透视投影 (45度视角, 16:9 宽高比, 近平面 0.1, 远平面 100)
            glm::mat4 projection = glm::perspective(glm::radians(70.0f), 1280.0f / 720.0f, 0.1f, 100.0f);

            // 【关键】Vulkan 的 Y 轴坐标系和 OpenGL 是反的，必须修补一下
            projection[1][1] *= -1;

            // 最终矩阵 = P * V * M
            glm::mat4 finalMatrix = projection * view * model;

            // 3. 渲染
            renderer.Render(&gpuMesh, finalMatrix);
        }

        // 退出时 renderer 会自动 Cleanup

    } catch (const std::exception& e) {
        LOG_CRITICAL("Crash: {}", e.what());
        return -1;
    }

    return 0;
}
