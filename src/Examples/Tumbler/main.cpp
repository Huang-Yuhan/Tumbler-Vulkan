#include "Core/Platform/AppWindow.h"
#include "Core/Utils/Log.h"
#include "Core/Assets/FMaterial.h"
#include "Core/Assets/FMaterialInstance.h"
#include "Core/Graphics/VulkanRenderer.h"
#include "AppLogic.h"
#include "Core/Assets/FAssetManager.h"
#include "Core/Editor/EditorSessionState.h"
#include "Core/Editor/UIManager.h"
#include "TumblerConsoleBindings.h"
#include "Core/GameSystem/Components/CCamera.h"
#include "Core/GameSystem/Components/CMeshRenderer.h"
#include "Core/GameSystem/Components/CTransform.h"
#include "Core/GameSystem/Components/CPointLight.h"
#include <imgui.h>
#include <algorithm>
#include <array>
#include <vector>
#include <chrono>
#include <charconv>
#include <string_view>
#include <system_error>
#include <utility>

#include "Core/GameSystem/FActor.h"
#include "Core/GameSystem/InputManager.h"
#include "Core/GameSystem/Components/CFirstPersonCamera.h"

namespace {
struct RuntimeTestOptions {
    bool ResizeStressTest = false;
    bool DescriptorStressTest = false;
    int MaxFrames = 240;
    int FramesPerResize = 20;
    int DescriptorStressRounds = 5;
    int DescriptorInstancesPerRound = 256;
};

int ParsePositiveIntOption(std::string_view argument, std::string_view prefix)
{
    const std::string_view valueText = argument.substr(prefix.size());
    int value = 0;
    const auto [ptr, ec] = std::from_chars(valueText.data(), valueText.data() + valueText.size(), value);
    if (ec != std::errc{} || ptr != valueText.data() + valueText.size() || value <= 0) {
        throw std::runtime_error("Invalid numeric option: " + std::string(argument));
    }

    return value;
}

RuntimeTestOptions ParseRuntimeTestOptions(int argc, char** argv)
{
    RuntimeTestOptions options;

    for (int i = 1; i < argc; ++i) {
        const std::string_view argument(argv[i]);
        if (argument == "--resize-stress-test") {
            options.ResizeStressTest = true;
        } else if (argument == "--descriptor-stress-test") {
            options.DescriptorStressTest = true;
        } else if (argument.starts_with("--test-max-frames=")) {
            options.MaxFrames = ParsePositiveIntOption(argument, "--test-max-frames=");
        } else if (argument.starts_with("--test-frames-per-resize=")) {
            options.FramesPerResize = ParsePositiveIntOption(argument, "--test-frames-per-resize=");
        } else if (argument.starts_with("--test-rounds=")) {
            options.DescriptorStressRounds = ParsePositiveIntOption(argument, "--test-rounds=");
        } else if (argument.starts_with("--test-instances-per-round=")) {
            options.DescriptorInstancesPerRound = ParsePositiveIntOption(argument, "--test-instances-per-round=");
        } else {
            throw std::runtime_error("Unknown command line option: " + std::string(argument));
        }
    }

    if (options.ResizeStressTest && options.DescriptorStressTest) {
        throw std::runtime_error("Resize and descriptor stress test modes are mutually exclusive.");
    }

    return options;
}

class ResizeStressTestRunner {
public:
    explicit ResizeStressTestRunner(RuntimeTestOptions options)
        : Options(options)
    {
        if (!Options.ResizeStressTest) {
            return;
        }

        const int minimumFrames = Options.FramesPerResize * static_cast<int>(ResizeSequence.size() + 1);
        Options.MaxFrames = std::max(Options.MaxFrames, minimumFrames);

        LOG_INFO("Resize stress test enabled. MaxFrames={}, FramesPerResize={}, Steps={}",
                 Options.MaxFrames,
                 Options.FramesPerResize,
                 ResizeSequence.size());
    }

    [[nodiscard]] bool IsEnabled() const
    {
        return Options.ResizeStressTest;
    }

    void NotifyResizeObserved(int width, int height)
    {
        if (!IsEnabled()) {
            return;
        }

        ++ObservedResizeEvents;
        LOG_INFO("Resize stress observed framebuffer resize {}/{} at {}x{}",
                 ObservedResizeEvents,
                 ResizeSequence.size(),
                 width,
                 height);
    }

    void OnFrameCompleted(AppWindow& window)
    {
        if (!IsEnabled() || Completed) {
            return;
        }

        ++FrameCount;

        if (FrameCount % Options.FramesPerResize == 0 && NextResizeIndex < ResizeSequence.size()) {
            const auto& [width, height] = ResizeSequence[NextResizeIndex];
            ++NextResizeIndex;
            LOG_INFO("Resize stress step {}/{} requesting {}x{}",
                     NextResizeIndex,
                     ResizeSequence.size(),
                     width,
                     height);
            window.SetWindowSize(width, height);
        }

        if (NextResizeIndex == ResizeSequence.size()
            && ObservedResizeEvents >= static_cast<int>(ResizeSequence.size())
            && FrameCount >= Options.FramesPerResize * static_cast<int>(ResizeSequence.size() + 1)) {
            Completed = true;
            LOG_INFO("Resize stress test completed successfully after {} frames.", FrameCount);
            window.RequestClose();
            return;
        }

        if (FrameCount >= Options.MaxFrames) {
            throw std::runtime_error("Resize stress test reached the frame limit before all resize events were observed.");
        }
    }

private:
    RuntimeTestOptions Options;
    int FrameCount = 0;
    int ObservedResizeEvents = 0;
    size_t NextResizeIndex = 0;
    bool Completed = false;

    static constexpr std::array<std::pair<int, int>, 6> ResizeSequence = {{
        {960, 540},
        {1400, 900},
        {1024, 768},
        {1600, 900},
        {800, 600},
        {1280, 720}
    }};
};

std::shared_ptr<FMaterialInstance> FindSourceMaterialInstance(FScene& scene, std::string& outActorName)
{
    for (const auto& actor : scene.GetAllActors()) {
        auto* meshRenderer = actor->GetComponent<CMeshRenderer>();
        if (meshRenderer == nullptr) {
            continue;
        }

        std::shared_ptr<FMaterialInstance> materialInstance = meshRenderer->GetMaterial();
        if (materialInstance != nullptr) {
            outActorName = actor->Name;
            return materialInstance;
        }
    }

    return nullptr;
}

class DescriptorStressTestRunner {
public:
    DescriptorStressTestRunner(RuntimeTestOptions options, FScene& scene, VulkanRenderer& renderer)
        : Options(options)
        , Scene(scene)
        , Renderer(renderer)
    {
        if (!Options.DescriptorStressTest) {
            return;
        }

        const int minimumFrames = Options.DescriptorStressRounds + 5;
        Options.MaxFrames = std::max(Options.MaxFrames, minimumFrames);

        LOG_INFO("Descriptor stress test enabled. Rounds={}, InstancesPerRound={}, MaxFrames={}",
                 Options.DescriptorStressRounds,
                 Options.DescriptorInstancesPerRound,
                 Options.MaxFrames);
    }

    [[nodiscard]] bool IsEnabled() const
    {
        return Options.DescriptorStressTest;
    }

    void RunRoundIfNeeded()
    {
        if (!IsEnabled() || CompletedRounds >= Options.DescriptorStressRounds) {
            return;
        }

        EnsureSourceMaterialResolved();

        std::vector<std::shared_ptr<FMaterialInstance>> transientInstances;
        transientInstances.reserve(static_cast<size_t>(Options.DescriptorInstancesPerRound));

        for (int index = 0; index < Options.DescriptorInstancesPerRound; ++index) {
            std::shared_ptr<FMaterialInstance> instance = SourceMaterial->CreateInstance();
            if (instance == nullptr) {
                throw std::runtime_error("Descriptor stress test failed to create a material instance.");
            }

            const float ratio = Options.DescriptorInstancesPerRound <= 1
                ? 0.0f
                : static_cast<float>(index) / static_cast<float>(Options.DescriptorInstancesPerRound - 1);

            instance->SetVector("BaseColorTint", glm::vec4(1.0f - ratio, 0.35f + ratio * 0.4f, ratio, 1.0f));
            instance->SetFloat("Roughness", 0.05f + ratio * 0.9f);
            instance->SetFloat("Metallic", (index % 2 == 0) ? 1.0f : 0.0f);
            instance->SetTwoSided((index % 3) == 0);
            instance->ApplyChanges();

            transientInstances.push_back(std::move(instance));
        }

        transientInstances.clear();
        ++CompletedRounds;
        PendingClose = CompletedRounds >= Options.DescriptorStressRounds;

        LOG_INFO("Descriptor stress round {}/{} completed. Instances={}, PendingDescriptorFrees={}",
                 CompletedRounds,
                 Options.DescriptorStressRounds,
                 Options.DescriptorInstancesPerRound,
                 Renderer.GetPendingDescriptorSetFreeCount());
    }

    void OnFrameCompleted(AppWindow& window)
    {
        if (!IsEnabled()) {
            return;
        }

        ++FrameCount;
        if (PendingClose) {
            PendingClose = false;
            LOG_INFO("Descriptor stress test completed successfully after {} frames.", FrameCount);
            window.RequestClose();
            return;
        }

        if (FrameCount >= Options.MaxFrames) {
            throw std::runtime_error("Descriptor stress test reached the frame limit before all rounds completed.");
        }
    }

private:
    void EnsureSourceMaterialResolved()
    {
        if (SourceMaterial != nullptr) {
            return;
        }

        std::shared_ptr<FMaterialInstance> sourceInstance = FindSourceMaterialInstance(Scene, SourceActorName);
        if (sourceInstance == nullptr) {
            throw std::runtime_error("Descriptor stress test could not find an actor with a mesh material instance.");
        }

        SourceMaterial = sourceInstance->GetParent();
        if (SourceMaterial == nullptr) {
            throw std::runtime_error("Descriptor stress test source material instance has no parent material.");
        }

        LOG_INFO("Descriptor stress test using source material from actor '{}'.", SourceActorName);
    }

    RuntimeTestOptions Options;
    FScene& Scene;
    VulkanRenderer& Renderer;
    std::shared_ptr<FMaterial> SourceMaterial;
    std::string SourceActorName;
    int FrameCount = 0;
    int CompletedRounds = 0;
    bool PendingClose = false;
};

}

int main(int argc, char** argv) {
    Log::Get().Init();
    LOG_INFO("Tumbler Engine Starting...");

    try {
        const RuntimeTestOptions runtimeTestOptions = ParseRuntimeTestOptions(argc, argv);
        ResizeStressTestRunner resizeStressTest(runtimeTestOptions);

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

        EditorSessionState editorSessionState;

        AppLogic logic;
        logic.Init(&renderer, &assetManager, &inputManager, &editorSessionState);

        DescriptorStressTestRunner descriptorStressTest(runtimeTestOptions, *logic.GetScene(), renderer);

        // 提前上传共用网格，防止渲染中途卡顿
        renderer.UploadMesh(assetManager.GetOrLoadMesh("DefaultPlane").get());

        // 3. 准备真实 DeltaTime 计时器
        auto currentTime = std::chrono::high_resolution_clock::now();

        // 4. UI 系统初始化
        UIManager ui_manager;
        ui_manager.Init(&window, &renderer, &inputManager);
        RegisterTumblerConsoleCommands(ui_manager.GetConsole(), *logic.GetScene(), *logic.GetMainCamera(), editorSessionState);

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
                int framebufferWidth = 0;
                int framebufferHeight = 0;
                window.GetFramebufferSize(framebufferWidth, framebufferHeight);
                window.ClearResizedFlag();
                resizeStressTest.NotifyResizeObserved(framebufferWidth, framebufferHeight);
                LOG_INFO("Window resized, recreating swapchain...");
            }

            // 更新输入系统
            inputManager.Tick();
            ui_manager.TickInput();

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
            viewData.RenderPath = editorSessionState.CurrentRenderPath;

            // --- C. 发送给底层渲染器执行 ---
            // 渲染器同时接收“视图”和“包裹”，并将 UI 录制指令作为回调传入
            descriptorStressTest.RunRoundIfNeeded();
            renderer.Render(viewData, renderPackets, [&](VkCommandBuffer cmd, uint32_t imgIdx) {
                ui_manager.RecordDrawCommands(cmd, &renderer, imgIdx);
            });

            resizeStressTest.OnFrameCompleted(window);
            descriptorStressTest.OnFrameCompleted(window);
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
