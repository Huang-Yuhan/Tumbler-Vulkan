#include "SceneLoader.h"

#include "Core/Assets/AssetDatabase.h"
#include "Core/GameSystem/Components/CCamera.h"
#include "Core/GameSystem/Components/CDirectionalLight.h"
#include "Core/GameSystem/Components/CPointLight.h"
#include "Core/GameSystem/Components/CStaticMesh.h"
#include "Core/GameSystem/FActor.h"
#include "Core/GameSystem/FScene.h"
#include "Core/Math/Quaternion.h"

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using namespace Tumbler::Math;

namespace Tumbler {

bool SceneLoader::LoadFromFile(::FScene& scene, const std::string& jsonPath, const AssetDatabase& assetDb,
                               Result& outResult) {
    std::ifstream file(jsonPath);
    if (!file.is_open()) {
        std::cerr << "[SceneLoader] Failed to open scene file: " << jsonPath << std::endl;
        return false;
    }

    json doc;
    try {
        doc = json::parse(file);
    } catch (const json::exception& e) {
        std::cerr << "[SceneLoader] JSON parse error: " << e.what() << std::endl;
        return false;
    }

    const std::string sceneName = doc.value("name", "Untitled");
    (void)sceneName;

    // ---- Camera ----
    if (doc.contains("camera") && doc["camera"].is_object()) {
        const auto& camJson = doc["camera"];
        auto* camActor = scene.CreateActor("MainCamera");
        auto* camera = camActor->AddComponent<CCamera>();

        camera->FOV = camJson.value("fov", 60.0f);
        camera->NearPlane = camJson.value("nearPlane", 0.1f);
        camera->FarPlane = camJson.value("farPlane", 1000.0f);

        if (camJson.contains("lookAt") && camJson["lookAt"].is_array() && camJson["lookAt"].size() >= 3) {
            camera->LookAt[0] = camJson["lookAt"][0];
            camera->LookAt[1] = camJson["lookAt"][1];
            camera->LookAt[2] = camJson["lookAt"][2];
        }

        if (camJson.contains("position") && camJson["position"].is_array() && camJson["position"].size() >= 3) {
            camActor->Transform.SetPosition(
                Vector3f(camJson["position"][0], camJson["position"][1], camJson["position"][2]));
        }

        outResult.CameraActor = camActor;
    }

    // ---- Objects ----
    if (doc.contains("objects") && doc["objects"].is_array()) {
        for (const auto& objJson : doc["objects"]) {
            const std::string name = objJson.value("name", "Unnamed");
            std::string meshSourcePath = objJson.value("mesh", "");

            if (meshSourcePath.empty()) {
                std::cerr << "[SceneLoader] Object '" << name << "' has no mesh path, skipping." << std::endl;
                continue;
            }

            std::string cookedMeshPath = assetDb.GetCookedPath(meshSourcePath, "meshes");
            if (cookedMeshPath.empty()) {
                std::cerr << "[SceneLoader] Mesh source path not in AssetDatabase: " << meshSourcePath << " (object '"
                          << name << "'), skipping." << std::endl;
                continue;
            }

            auto* actor = scene.CreateActor(name);
            auto* staticMesh = actor->AddComponent<CStaticMesh>();
            staticMesh->MeshSourcePath = meshSourcePath;
            staticMesh->CookedMeshPath = cookedMeshPath;

            // 材质数组
            if (objJson.contains("materials") && objJson["materials"].is_array()) {
                for (const auto& matEntry : objJson["materials"]) {
                    if (matEntry.is_null()) {
                        staticMesh->AddMaterialOverride("");
                    } else {
                        staticMesh->AddMaterialOverride(matEntry.get<std::string>());
                    }
                }
            }

            // Transform
            if (objJson.contains("transform") && objJson["transform"].is_object()) {
                auto& t = actor->Transform;
                const auto& trJson = objJson["transform"];

                if (trJson.contains("position") && trJson["position"].is_array() && trJson["position"].size() >= 3) {
                    t.SetPosition(Vector3f(trJson["position"][0], trJson["position"][1], trJson["position"][2]));
                }
                if (trJson.contains("scale") && trJson["scale"].is_array() && trJson["scale"].size() >= 3) {
                    t.SetScale(Vector3f(trJson["scale"][0], trJson["scale"][1], trJson["scale"][2]));
                }
                if (trJson.contains("rotation") && trJson["rotation"].is_array() && trJson["rotation"].size() >= 4) {
                    t.SetRotation(Quaternionf(trJson["rotation"][0], trJson["rotation"][1], trJson["rotation"][2],
                                              trJson["rotation"][3]));
                }
            }

            outResult.MeshActors.push_back(actor);
        }
    }

    // ---- Lights ----
    if (doc.contains("lights") && doc["lights"].is_array()) {
        for (const auto& lightJson : doc["lights"]) {
            const std::string lightType = lightJson.value("type", "");

            if (lightType == "directional") {
                auto* actor = scene.CreateActor("DirectionalLight");
                auto* light = actor->AddComponent<CDirectionalLight>();

                if (lightJson.contains("direction") && lightJson["direction"].is_array() &&
                    lightJson["direction"].size() >= 3) {
                    light->Direction[0] = lightJson["direction"][0];
                    light->Direction[1] = lightJson["direction"][1];
                    light->Direction[2] = lightJson["direction"][2];
                }
                if (lightJson.contains("color") && lightJson["color"].is_array() && lightJson["color"].size() >= 3) {
                    light->Color[0] = lightJson["color"][0];
                    light->Color[1] = lightJson["color"][1];
                    light->Color[2] = lightJson["color"][2];
                }
                light->Intensity = lightJson.value("intensity", 1.0f);

                outResult.LightActors.push_back(actor);

            } else if (lightType == "point") {
                auto* actor = scene.CreateActor("PointLight");
                auto* light = actor->AddComponent<CPointLight>();

                if (lightJson.contains("position") && lightJson["position"].is_array() &&
                    lightJson["position"].size() >= 3) {
                    actor->Transform.SetPosition(
                        Vector3f(lightJson["position"][0], lightJson["position"][1], lightJson["position"][2]));
                }
                if (lightJson.contains("color") && lightJson["color"].is_array() && lightJson["color"].size() >= 3) {
                    light->Color[0] = lightJson["color"][0];
                    light->Color[1] = lightJson["color"][1];
                    light->Color[2] = lightJson["color"][2];
                }
                light->Intensity = lightJson.value("intensity", 50.0f);
                light->Range = lightJson.value("range", 20.0f);

                outResult.LightActors.push_back(actor);
            } else {
                std::cerr << "[SceneLoader] Unknown light type: " << lightType << std::endl;
            }
        }
    }

    std::cout << "[SceneLoader] Loaded scene '" << sceneName << "': " << outResult.MeshActors.size() << " meshes, "
              << outResult.LightActors.size() << " lights" << std::endl;

    return true;
}

} // namespace Tumbler
