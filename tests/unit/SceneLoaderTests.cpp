#include "Core/Assets/AssetDatabase.h"
#include "Core/GameSystem/Components/CCamera.h"
#include "Core/GameSystem/Components/CDirectionalLight.h"
#include "Core/GameSystem/Components/CPointLight.h"
#include "Core/GameSystem/Components/CStaticMesh.h"
#include "Core/GameSystem/FActor.h"
#include "Core/GameSystem/FScene.h"
#include "Core/Scene/SceneLoader.h"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

using namespace Tumbler;

namespace {

class SceneLoaderTests : public ::testing::Test {
protected:
    void SetUp() override {
        // 写入临时 asset_map.json
        m_AssetMapPath = (std::filesystem::temp_directory_path() / "tumbler_test_scene_asset_map.json").string();
        std::ofstream amFile(m_AssetMapPath);
        amFile << R"({
  "version": 1,
  "meshes": {
    "assets/models/test_cube.obj": {
      "cooked": "cooked/meshes/test_cube.tmesh",
      "subMeshCount": 2,
      "sourceHash": 123
    }
  },
  "textures": {},
  "materials": {
    "assets/materials/red.tmat": {
      "cooked": "cooked/materials/red.tmat",
      "sourceHash": 456
    },
    "assets/materials/blue.tmat": {
      "cooked": "cooked/materials/blue.tmat",
      "sourceHash": 789
    }
  }
})";
        amFile.close();
        m_AssetDb.LoadAssetMap(m_AssetMapPath);

        // 写入 Scene JSON
        m_ScenePath = (std::filesystem::temp_directory_path() / "tumbler_test_scene.json").string();
        std::ofstream sceneFile(m_ScenePath);
        sceneFile << R"({
  "version": 1,
  "name": "TestScene",
  "camera": {
    "position": [0, 2, 8],
    "lookAt": [0, 0, 0],
    "fov": 60.0
  },
  "objects": [
    {
      "name": "TestCube_Red",
      "mesh": "assets/models/test_cube.obj",
      "materials": [
        "assets/materials/red.tmat",
        null
      ],
      "transform": {
        "position": [1, 2, 3],
        "rotation": [0, 0, 0, 1],
        "scale": [2, 2, 2]
      }
    },
    {
      "name": "TestCube_Blue",
      "mesh": "assets/models/test_cube.obj",
      "materials": [],
      "transform": {
        "position": [4, 5, 6],
        "rotation": [0, 1, 0, 0],
        "scale": [1, 1, 1]
      }
    }
  ],
  "lights": [
    {
      "type": "directional",
      "direction": [0, -1, 0],
      "color": [1, 1, 1],
      "intensity": 1.5
    },
    {
      "type": "point",
      "position": [0, 4, 0],
      "color": [1, 0.5, 0],
      "intensity": 100.0,
      "range": 30.0
    }
  ]
})";
        sceneFile.close();
    }

    void TearDown() override {
        std::remove(m_ScenePath.c_str());
        std::remove(m_AssetMapPath.c_str());
    }

    AssetDatabase m_AssetDb;
    std::string m_ScenePath;
    std::string m_AssetMapPath;
};

TEST_F(SceneLoaderTests, LoadsSceneAndCreatesActors) {
    FScene scene;
    SceneLoader loader;
    SceneLoader::Result result;

    ASSERT_TRUE(loader.LoadFromFile(scene, m_ScenePath, m_AssetDb, result));

    // Camera
    ASSERT_NE(result.CameraActor, nullptr);
    auto* camera = result.CameraActor->GetComponent<CCamera>();
    ASSERT_NE(camera, nullptr);
    EXPECT_FLOAT_EQ(camera->FOV, 60.0f);
    EXPECT_FLOAT_EQ(camera->LookAt[0], 0.0f);
    EXPECT_FLOAT_EQ(camera->LookAt[1], 0.0f);
    EXPECT_FLOAT_EQ(camera->LookAt[2], 0.0f);

    // Meshes
    ASSERT_EQ(result.MeshActors.size(), 2u);

    // First Mesh Actor
    auto* meshActor1 = result.MeshActors[0];
    EXPECT_EQ(meshActor1->Name, "TestCube_Red");
    auto* sm1 = meshActor1->GetComponent<CStaticMesh>();
    ASSERT_NE(sm1, nullptr);
    EXPECT_EQ(sm1->MeshSourcePath, "assets/models/test_cube.obj");
    EXPECT_EQ(sm1->CookedMeshPath, "cooked/meshes/test_cube.tmesh");
    ASSERT_EQ(sm1->MaterialOverrides.size(), 2u);
    EXPECT_EQ(sm1->MaterialOverrides[0].SourcePath, "assets/materials/red.tmat");
    EXPECT_EQ(sm1->MaterialOverrides[1].SourcePath, ""); // null → empty

    // Transform of first mesh
    EXPECT_FLOAT_EQ(meshActor1->Transform.GetPosition().X, 1.0f);
    EXPECT_FLOAT_EQ(meshActor1->Transform.GetPosition().Y, 2.0f);
    EXPECT_FLOAT_EQ(meshActor1->Transform.GetPosition().Z, 3.0f);
    EXPECT_FLOAT_EQ(meshActor1->Transform.GetScale().X, 2.0f);

    // Second Mesh Actor
    auto* meshActor2 = result.MeshActors[1];
    EXPECT_EQ(meshActor2->Name, "TestCube_Blue");
    auto* sm2 = meshActor2->GetComponent<CStaticMesh>();
    ASSERT_NE(sm2, nullptr);
    EXPECT_TRUE(sm2->MaterialOverrides.empty());

    // Lights
    ASSERT_EQ(result.LightActors.size(), 2u);

    // Directional light
    auto* dirLightActor = result.LightActors[0];
    EXPECT_EQ(dirLightActor->Name, "DirectionalLight");
    auto* dirLight = dirLightActor->GetComponent<CDirectionalLight>();
    ASSERT_NE(dirLight, nullptr);
    EXPECT_FLOAT_EQ(dirLight->Direction[0], 0.0f);
    EXPECT_FLOAT_EQ(dirLight->Direction[1], -1.0f);
    EXPECT_FLOAT_EQ(dirLight->Intensity, 1.5f);

    // Point light
    auto* ptLightActor = result.LightActors[1];
    EXPECT_EQ(ptLightActor->Name, "PointLight");
    auto* ptLight = ptLightActor->GetComponent<CPointLight>();
    ASSERT_NE(ptLight, nullptr);
    EXPECT_FLOAT_EQ(ptLight->Color[0], 1.0f);
    EXPECT_FLOAT_EQ(ptLight->Intensity, 100.0f);
    EXPECT_FLOAT_EQ(ptLight->Range, 30.0f);
}

TEST_F(SceneLoaderTests, MissingFileReturnsFalse) {
    FScene scene;
    SceneLoader loader;
    SceneLoader::Result result;

    EXPECT_FALSE(loader.LoadFromFile(scene, (std::filesystem::temp_directory_path() / "nonexistent_scene.json").string(), m_AssetDb, result));
}

TEST_F(SceneLoaderTests, MeshNotInAssetDatabaseSkipsObject) {
    // 写一个引用未知 mesh 的 scene
    std::string badScenePath = (std::filesystem::temp_directory_path() / "tumbler_test_bad_scene.json").string();
    std::ofstream f(badScenePath);
    f << R"({
  "version": 1,
  "name": "BadScene",
  "objects": [
    {
      "name": "UnknownMesh",
      "mesh": "assets/models/nonexistent.obj",
      "materials": []
    }
  ]
})";
    f.close();

    FScene scene;
    SceneLoader loader;
    SceneLoader::Result result;
    loader.LoadFromFile(scene, badScenePath, m_AssetDb, result);

    // 未知 mesh 应该被跳过
    EXPECT_TRUE(result.MeshActors.empty());

    std::remove(badScenePath.c_str());
}

TEST_F(SceneLoaderTests, CameraDefaultsWhenNotProvided) {
    std::string noCamPath = (std::filesystem::temp_directory_path() / "tumbler_test_no_cam.json").string();
    std::ofstream f(noCamPath);
    f << R"({"version":1,"name":"NoCamera","objects":[]})";
    f.close();

    FScene scene;
    SceneLoader loader;
    SceneLoader::Result result;
    EXPECT_TRUE(loader.LoadFromFile(scene, noCamPath, m_AssetDb, result));
    EXPECT_EQ(result.CameraActor, nullptr);

    std::remove(noCamPath.c_str());
}

} // namespace
