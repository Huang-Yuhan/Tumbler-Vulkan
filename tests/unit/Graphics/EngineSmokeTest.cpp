#include "Core/Engine/Engine.h"
#include "Core/Engine/EngineConfig.h"

#include <GLFW/glfw3.h>
#include <gtest/gtest.h>
#include <vulkan/vulkan.h>

#include <vector>

namespace {

// 简单的 Vulkan 环境检测
void SkipIfNoVulkan() {
    uint32_t version = 0;
    if (vkEnumerateInstanceVersion(&version) != VK_SUCCESS || version < VK_API_VERSION_1_4) {
        GTEST_SKIP() << "Vulkan 1.4 not available";
    }
    if (!glfwInit()) {
        GTEST_SKIP() << "GLFW init failed";
    }
    if (!glfwVulkanSupported()) {
        glfwTerminate();
        GTEST_SKIP() << "GLFW reports Vulkan unsupported";
    }
    glfwTerminate();
}

class EngineSmokeTest : public ::testing::Test {
protected:
    void SetUp() override {
        SkipIfNoVulkan();

        // 使用隐藏窗口 + 最小分辨率
        m_Config.WindowWidth = 64;
        m_Config.WindowHeight = 64;
        m_Config.WindowTitle = "TumblerEngineSmokeTest";
        // Asset map 不存在时 Engine 应正常启动（仅 warning）
        m_Config.AssetMapPath = "/tmp/nonexistent_asset_map.json";
    }

    void TearDown() override {
        if (m_Engine) {
            m_Engine->Shutdown();
            m_Engine.reset();
        }
    }

    std::unique_ptr<Tumbler::Engine> m_Engine;
    Tumbler::EngineConfig m_Config;
};

TEST_F(EngineSmokeTest, InitCreatesAllSubsystems) {
    m_Engine = std::make_unique<Tumbler::Engine>();
    ASSERT_TRUE(m_Engine->Init(m_Config));

    // 验证所有子系统都创建成功
    EXPECT_NE(m_Engine->GetWindow(), nullptr);
    EXPECT_NE(m_Engine->GetAssetDatabase(), nullptr);
    EXPECT_NE(m_Engine->GetVulkanContext(), nullptr);
    EXPECT_NE(m_Engine->GetRenderDevice(), nullptr);
    EXPECT_NE(m_Engine->GetCommandManager(), nullptr);
    EXPECT_NE(m_Engine->GetSwapchain(), nullptr);
    EXPECT_NE(m_Engine->GetDescriptorManager(), nullptr);
    EXPECT_NE(m_Engine->GetResourceManager(), nullptr);
}

TEST_F(EngineSmokeTest, InitAndShutdownDoesNotCrash) {
    m_Engine = std::make_unique<Tumbler::Engine>();
    ASSERT_TRUE(m_Engine->Init(m_Config));
    m_Engine->Shutdown();
    EXPECT_TRUE(true); // No crash = pass
    m_Engine.reset();  // 避免 TearDown 里再次 Shutdown
}

} // namespace
