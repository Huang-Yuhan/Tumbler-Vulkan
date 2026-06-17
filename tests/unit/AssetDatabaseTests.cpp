#include "Core/Assets/AssetDatabase.h"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

using namespace Tumbler;

namespace {

class AssetDatabaseTests : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建临时 asset_map.json
        m_TestPath = (std::filesystem::temp_directory_path() / "tumbler_test_asset_map.json").string();
        WriteTestAssetMap();
    }

    void TearDown() override { std::remove(m_TestPath.c_str()); }

    void WriteTestAssetMap() {
        std::ofstream file(m_TestPath);
        file << R"({
  "version": 1,
  "meshes": {
    "assets/models/bunny.obj": {
      "cooked": "cooked/meshes/bunny.tmesh",
      "subMeshCount": 3,
      "sourceHash": 12345678
    },
    "assets/models/cube.obj": {
      "cooked": "cooked/meshes/cube.tmesh",
      "subMeshCount": 1,
      "sourceHash": 87654321
    }
  },
  "textures": {
    "assets/textures/wood.png": {
      "cooked": "cooked/textures/wood.ttex",
      "format": "2",
      "mipLevels": 5,
      "sourceHash": 11111111
    },
    "assets/textures/brick.png": {
      "cooked": "cooked/textures/brick.ttex",
      "format": "3",
      "mipLevels": 7,
      "sourceHash": 22222222
    }
  },
  "materials": {
    "assets/materials/wall.tmat": {
      "cooked": "cooked/materials/wall.tmat",
      "sourceHash": 33333333,
      "dependsOn": [
        "assets/textures/wood.png",
        "assets/textures/brick.png"
      ]
    }
  }
})";
    }

    std::string m_TestPath;
};

TEST_F(AssetDatabaseTests, LoadsAssetMapAndReturnsIsLoaded) {
    AssetDatabase db;
    EXPECT_FALSE(db.IsLoaded());
    EXPECT_TRUE(db.LoadAssetMap(m_TestPath));
    EXPECT_TRUE(db.IsLoaded());
}

TEST_F(AssetDatabaseTests, MissingFileReturnsFalse) {
    AssetDatabase db;
    EXPECT_FALSE(db.LoadAssetMap((std::filesystem::temp_directory_path() / "nonexistent_file.json").string()));
    EXPECT_FALSE(db.IsLoaded());
}

TEST_F(AssetDatabaseTests, GetCookedPathReturnsCorrectPaths) {
    AssetDatabase db;
    db.LoadAssetMap(m_TestPath);

    EXPECT_EQ(db.GetCookedPath("assets/models/bunny.obj", "meshes"), "cooked/meshes/bunny.tmesh");
    EXPECT_EQ(db.GetCookedPath("assets/textures/wood.png", "textures"), "cooked/textures/wood.ttex");
    EXPECT_EQ(db.GetCookedPath("assets/materials/wall.tmat", "materials"), "cooked/materials/wall.tmat");
}

TEST_F(AssetDatabaseTests, GetCookedPathReturnsEmptyForUnknownSource) {
    AssetDatabase db;
    db.LoadAssetMap(m_TestPath);

    EXPECT_EQ(db.GetCookedPath("assets/models/unknown.obj", "meshes"), "");
    EXPECT_EQ(db.GetCookedPath("path/not/in/map", "textures"), "");
}

TEST_F(AssetDatabaseTests, CountsReturnCorrectSizes) {
    AssetDatabase db;
    db.LoadAssetMap(m_TestPath);

    EXPECT_EQ(db.GetMeshCount(), 2u);
    EXPECT_EQ(db.GetTextureCount(), 2u);
    EXPECT_EQ(db.GetMaterialCount(), 1u);
}

TEST_F(AssetDatabaseTests, GetMeshMetaReturnsCorrectData) {
    AssetDatabase db;
    db.LoadAssetMap(m_TestPath);

    const auto* meta = db.GetMeshMeta("assets/models/bunny.obj");
    ASSERT_NE(meta, nullptr);
    EXPECT_EQ(meta->CookedPath, "cooked/meshes/bunny.tmesh");
    EXPECT_EQ(meta->SubMeshCount, 3u);
    EXPECT_EQ(meta->SourceHash, 12345678u);
}

TEST_F(AssetDatabaseTests, GetMeshMetaReturnsNullForUnknown) {
    AssetDatabase db;
    db.LoadAssetMap(m_TestPath);

    EXPECT_EQ(db.GetMeshMeta("unknown.obj"), nullptr);
}

TEST_F(AssetDatabaseTests, GetTextureMetaReturnsCorrectData) {
    AssetDatabase db;
    db.LoadAssetMap(m_TestPath);

    const auto* meta = db.GetTextureMeta("assets/textures/brick.png");
    ASSERT_NE(meta, nullptr);
    EXPECT_EQ(meta->CookedPath, "cooked/textures/brick.ttex");
    EXPECT_EQ(meta->Format, "3");
    EXPECT_EQ(meta->MipLevels, 7u);
    EXPECT_EQ(meta->SourceHash, 22222222u);
}

TEST_F(AssetDatabaseTests, GetMaterialMetaDependsOn) {
    AssetDatabase db;
    db.LoadAssetMap(m_TestPath);

    const auto* meta = db.GetMaterialMeta("assets/materials/wall.tmat");
    ASSERT_NE(meta, nullptr);
    ASSERT_EQ(meta->DependsOn.size(), 2u);
    EXPECT_EQ(meta->DependsOn[0], "assets/textures/wood.png");
    EXPECT_EQ(meta->DependsOn[1], "assets/textures/brick.png");
}

TEST_F(AssetDatabaseTests, GetAllSourcesReturnsAllKeys) {
    AssetDatabase db;
    db.LoadAssetMap(m_TestPath);

    auto meshes = db.GetAllSources("meshes");
    EXPECT_EQ(meshes.size(), 2u);
    auto textures = db.GetAllSources("textures");
    EXPECT_EQ(textures.size(), 2u);
    auto materials = db.GetAllSources("materials");
    EXPECT_EQ(materials.size(), 1u);
}

TEST_F(AssetDatabaseTests, GetAllSourcesUnknownTypeReturnsEmpty) {
    AssetDatabase db;
    db.LoadAssetMap(m_TestPath);

    auto result = db.GetAllSources("animations");
    EXPECT_TRUE(result.empty());
}

TEST_F(AssetDatabaseTests, LoadAssetMapOverwritesPreviousData) {
    AssetDatabase db;
    db.LoadAssetMap(m_TestPath);
    EXPECT_EQ(db.GetMeshCount(), 2u);

    // 第二次加载同一文件 → 数据覆盖（不是重复）
    db.LoadAssetMap(m_TestPath);
    EXPECT_EQ(db.GetMeshCount(), 2u);
}

} // namespace
