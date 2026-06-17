#include "Core/Engine/EngineConfig.h"

#include <fstream>
#include <gtest/gtest.h>

using namespace Tumbler;

namespace {

class EngineConfigTests : public ::testing::Test {
protected:
    void SetUp() override { m_TestPath = "/tmp/tumbler_test_engine.json"; }

    void TearDown() override { std::remove(m_TestPath.c_str()); }

    void WriteConfig(const std::string& json) {
        std::ofstream f(m_TestPath);
        f << json;
    }

    std::string m_TestPath;
};

TEST_F(EngineConfigTests, DefaultValuesBeforeLoad) {
    EngineConfig cfg;
    EXPECT_EQ(cfg.WindowWidth, 1280);
    EXPECT_EQ(cfg.WindowHeight, 720);
    EXPECT_EQ(cfg.WindowTitle, "Tumbler");
    EXPECT_EQ(cfg.CookedPath, "cooked/");
}

TEST_F(EngineConfigTests, LoadsWindowSettings) {
    WriteConfig(R"({"window": {"width": 1920, "height": 1080, "title": "TestApp"}})");

    EngineConfig cfg;
    ASSERT_TRUE(cfg.LoadFromFile(m_TestPath));
    EXPECT_EQ(cfg.WindowWidth, 1920);
    EXPECT_EQ(cfg.WindowHeight, 1080);
    EXPECT_EQ(cfg.WindowTitle, "TestApp");
}

TEST_F(EngineConfigTests, LoadsRenderSettings) {
    WriteConfig(R"({"render": {"cookedPath": "out/cooked/", "assetMap": "out/asset_map.json"}})");

    EngineConfig cfg;
    ASSERT_TRUE(cfg.LoadFromFile(m_TestPath));
    EXPECT_EQ(cfg.CookedPath, "out/cooked/");
    EXPECT_EQ(cfg.AssetMapPath, "out/asset_map.json");
}

TEST_F(EngineConfigTests, MissingFileReturnsFalse) {
    EngineConfig cfg;
    EXPECT_FALSE(cfg.LoadFromFile("/tmp/nonexistent_engine_config.json"));
}

TEST_F(EngineConfigTests, PartialOverridesKeepDefaults) {
    WriteConfig(R"({"window": {"width": 800}})");

    EngineConfig cfg;
    ASSERT_TRUE(cfg.LoadFromFile(m_TestPath));
    EXPECT_EQ(cfg.WindowWidth, 800);
    EXPECT_EQ(cfg.WindowHeight, 720);      // 保持默认
    EXPECT_EQ(cfg.WindowTitle, "Tumbler"); // 保持默认
    EXPECT_EQ(cfg.CookedPath, "cooked/");  // 保持默认
}

TEST_F(EngineConfigTests, EmptyJsonKeepsAllDefaults) {
    WriteConfig(R"({})");

    EngineConfig cfg;
    ASSERT_TRUE(cfg.LoadFromFile(m_TestPath));
    EXPECT_EQ(cfg.WindowWidth, 1280);
    EXPECT_EQ(cfg.WindowHeight, 720);
    EXPECT_EQ(cfg.CookedPath, "cooked/");
}

} // namespace
