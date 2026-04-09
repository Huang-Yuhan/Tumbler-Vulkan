#include "TumblerConsoleBindings.h"

#include "Core/Editor/EditorSessionState.h"
#include "Core/Editor/RuntimeConsole.h"
#include "Core/GameSystem/FActor.h"
#include "Core/GameSystem/FScene.h"
#include "Core/GameSystem/Components/CFirstPersonCamera.h"
#include "Core/GameSystem/Components/CMeshRenderer.h"
#include "Core/GameSystem/Components/CPointLight.h"

#include <glm/vec3.hpp>

#include <algorithm>
#include <cctype>
#include <exception>
#include <string>
#include <vector>

namespace {
std::string ToLowerCopy(const std::string& text)
{
    std::string lower = text;
    std::ranges::transform(lower, lower.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return lower;
}

bool TryParseFloat(const std::string& text, float& outValue)
{
    try {
        size_t parsedCharacters = 0;
        outValue = std::stof(text, &parsedCharacters);
        return parsedCharacters == text.size();
    } catch (const std::exception&) {
        return false;
    }
}

bool ValidateSelectedActor(FScene& scene, EditorSessionState& editorState, RuntimeConsole& console)
{
    if (editorState.SelectedActor == nullptr) {
        return false;
    }

    if (!scene.ContainsActor(editorState.SelectedActor)) {
        console.AddMessage(EConsoleMessageType::Warning, "Selected actor is no longer valid. Clearing selection.");
        editorState.SelectedActor = nullptr;
        return false;
    }

    return true;
}

FActor* ResolveActorReference(
    FScene& scene,
    EditorSessionState& editorState,
    RuntimeConsole& console,
    const std::string& token)
{
    if (ToLowerCopy(token) == "selected") {
        if (!ValidateSelectedActor(scene, editorState, console)) {
            console.AddMessage(EConsoleMessageType::Error, "No actor is currently selected.");
            return nullptr;
        }
        return editorState.SelectedActor;
    }

    FActor* actor = scene.FindActorByName(token);
    if (actor == nullptr) {
        console.AddMessage(EConsoleMessageType::Error, "Actor not found: " + token);
    }
    return actor;
}

bool ParseVec3Arguments(
    RuntimeConsole& console,
    const std::vector<std::string>& args,
    size_t startIndex,
    const std::string& usage,
    glm::vec3& outVector)
{
    if (args.size() < startIndex + 3) {
        console.AddMessage(EConsoleMessageType::Error, "Usage: " + usage);
        return false;
    }

    float values[3] = {};
    for (size_t axis = 0; axis < 3; ++axis) {
        if (!TryParseFloat(args[startIndex + axis], values[axis])) {
            console.AddMessage(EConsoleMessageType::Error, "Invalid number: " + args[startIndex + axis]);
            return false;
        }
    }

    outVector = glm::vec3(values[0], values[1], values[2]);
    return true;
}

void SetActorEulerRotation(FActor* actor, const glm::vec3& eulerDegrees)
{
    if (actor == nullptr) {
        return;
    }

    if (auto* cameraComponent = actor->GetComponent<CFirstPersonCamera>()) {
        cameraComponent->SetLookEuler(eulerDegrees);
        return;
    }

    actor->Transform.SetRotation(eulerDegrees);
}

void AddUsage(RuntimeConsole& console, const std::string& usage)
{
    console.AddMessage(EConsoleMessageType::Error, "Usage: " + usage);
}

std::vector<std::string> CollectActorNameSuggestions(
    FScene& scene,
    EditorSessionState& editorState,
    bool includeSelectedToken,
    bool includeNoneToken,
    const FActor* excludedActor = nullptr)
{
    std::vector<std::string> suggestions;

    if (includeSelectedToken
        && editorState.SelectedActor != nullptr
        && editorState.SelectedActor != excludedActor
        && scene.ContainsActor(editorState.SelectedActor)) {
        suggestions.push_back("selected");
    }

    if (includeNoneToken) {
        suggestions.push_back("none");
    }

    for (const auto& actor : scene.GetAllActors()) {
        if (actor.get() == excludedActor) {
            continue;
        }
        suggestions.push_back(actor->Name);
    }

    return suggestions;
}

std::vector<std::string> CollectPointLightActorSuggestions(FScene& scene)
{
    std::vector<std::string> suggestions;
    for (const auto& actor : scene.GetAllActors()) {
        if (actor->GetComponent<CPointLight>() != nullptr) {
            suggestions.push_back(actor->Name);
        }
    }

    return suggestions;
}
}

void RegisterTumblerConsoleCommands(
    RuntimeConsole& console,
    FScene& scene,
    CFirstPersonCamera& camera,
    EditorSessionState& editorState)
{
    console.RegisterCommand({
        .Name = "actors",
        .Usage = "actors",
        .Description = "List every actor currently in the scene.",
        .Handler = [&console, &scene](const std::vector<std::string>& args) {
            if (!args.empty()) {
                AddUsage(console, "actors");
                return;
            }

            const auto& actors = scene.GetAllActors();
            console.AddMessage(EConsoleMessageType::Info, "Actors in scene: " + std::to_string(actors.size()));
            for (const auto& actor : actors) {
                console.AddMessage(EConsoleMessageType::Info, "  " + actor->Name);
            }
        }
    });

    console.RegisterCommand({
        .Name = "select",
        .Usage = "select <ActorName|none>",
        .Description = "Select an actor for inspector editing and selected-target commands.",
        .AutocompleteHandler = [&scene, &editorState](const std::vector<std::string>&, size_t activeArgIndex) {
            if (activeArgIndex != 0) {
                return std::vector<std::string>{};
            }

            return CollectActorNameSuggestions(scene, editorState, false, true);
        },
        .Handler = [&console, &scene, &editorState](const std::vector<std::string>& args) {
            if (args.size() != 1) {
                AddUsage(console, "select <ActorName|none>");
                return;
            }

            if (ToLowerCopy(args[0]) == "none") {
                editorState.SelectedActor = nullptr;
                console.AddMessage(EConsoleMessageType::Info, "Selection cleared.");
                return;
            }

            FActor* actor = scene.FindActorByName(args[0]);
            if (actor == nullptr) {
                console.AddMessage(EConsoleMessageType::Error, "Actor not found: " + args[0]);
                return;
            }

            editorState.SelectedActor = actor;
            console.AddMessage(EConsoleMessageType::Info, "Selected actor: " + actor->Name);
        }
    });

    console.RegisterCommand({
        .Name = "actor.move",
        .Usage = "actor.move <ActorName|selected> <x> <y> <z>",
        .Description = "Set an actor world position.",
        .AutocompleteHandler = [&scene, &editorState](const std::vector<std::string>&, size_t activeArgIndex) {
            if (activeArgIndex != 0) {
                return std::vector<std::string>{};
            }

            return CollectActorNameSuggestions(scene, editorState, true, false);
        },
        .Handler = [&console, &scene, &editorState](const std::vector<std::string>& args) {
            if (args.size() != 4) {
                AddUsage(console, "actor.move <ActorName|selected> <x> <y> <z>");
                return;
            }

            FActor* actor = ResolveActorReference(scene, editorState, console, args[0]);
            if (actor == nullptr) {
                return;
            }

            glm::vec3 position{};
            if (!ParseVec3Arguments(console, args, 1, "actor.move <ActorName|selected> <x> <y> <z>", position)) {
                return;
            }

            actor->Transform.SetPosition(position);
            console.AddMessage(EConsoleMessageType::Info, "Moved actor: " + actor->Name);
        }
    });

    console.RegisterCommand({
        .Name = "actor.rotate",
        .Usage = "actor.rotate <ActorName|selected> <pitch> <yaw> <roll>",
        .Description = "Set an actor rotation in Euler degrees.",
        .AutocompleteHandler = [&scene, &editorState](const std::vector<std::string>&, size_t activeArgIndex) {
            if (activeArgIndex != 0) {
                return std::vector<std::string>{};
            }

            return CollectActorNameSuggestions(scene, editorState, true, false);
        },
        .Handler = [&console, &scene, &editorState](const std::vector<std::string>& args) {
            if (args.size() != 4) {
                AddUsage(console, "actor.rotate <ActorName|selected> <pitch> <yaw> <roll>");
                return;
            }

            FActor* actor = ResolveActorReference(scene, editorState, console, args[0]);
            if (actor == nullptr) {
                return;
            }

            glm::vec3 rotation{};
            if (!ParseVec3Arguments(console, args, 1, "actor.rotate <ActorName|selected> <pitch> <yaw> <roll>", rotation)) {
                return;
            }

            SetActorEulerRotation(actor, rotation);
            console.AddMessage(EConsoleMessageType::Info, "Rotated actor: " + actor->Name);
        }
    });

    console.RegisterCommand({
        .Name = "actor.scale",
        .Usage = "actor.scale <ActorName|selected> <x> <y> <z>",
        .Description = "Set an actor scale.",
        .AutocompleteHandler = [&scene, &editorState](const std::vector<std::string>&, size_t activeArgIndex) {
            if (activeArgIndex != 0) {
                return std::vector<std::string>{};
            }

            return CollectActorNameSuggestions(scene, editorState, true, false);
        },
        .Handler = [&console, &scene, &editorState](const std::vector<std::string>& args) {
            if (args.size() != 4) {
                AddUsage(console, "actor.scale <ActorName|selected> <x> <y> <z>");
                return;
            }

            FActor* actor = ResolveActorReference(scene, editorState, console, args[0]);
            if (actor == nullptr) {
                return;
            }

            glm::vec3 scale{};
            if (!ParseVec3Arguments(console, args, 1, "actor.scale <ActorName|selected> <x> <y> <z>", scale)) {
                return;
            }

            actor->Transform.SetScale(scale);
            console.AddMessage(EConsoleMessageType::Info, "Scaled actor: " + actor->Name);
        }
    });

    console.RegisterCommand({
        .Name = "camera.pos",
        .Usage = "camera.pos <x> <y> <z>",
        .Description = "Move the main first-person camera.",
        .Handler = [&console, &camera](const std::vector<std::string>& args) {
            if (args.size() != 3) {
                AddUsage(console, "camera.pos <x> <y> <z>");
                return;
            }

            glm::vec3 position{};
            if (!ParseVec3Arguments(console, args, 0, "camera.pos <x> <y> <z>", position)) {
                return;
            }

            camera.GetOwner()->Transform.SetPosition(position);
            console.AddMessage(EConsoleMessageType::Info, "Updated camera position.");
        }
    });

    console.RegisterCommand({
        .Name = "camera.speed",
        .Usage = "camera.speed <value>",
        .Description = "Set the main camera move speed.",
        .Handler = [&console, &camera](const std::vector<std::string>& args) {
            if (args.size() != 1) {
                AddUsage(console, "camera.speed <value>");
                return;
            }

            float speed = 0.0f;
            if (!TryParseFloat(args[0], speed)) {
                console.AddMessage(EConsoleMessageType::Error, "Invalid number: " + args[0]);
                return;
            }

            if (speed < 0.0f) {
                console.AddMessage(EConsoleMessageType::Error, "Camera speed must be non-negative.");
                return;
            }

            camera.MoveSpeed = speed;
            console.AddMessage(EConsoleMessageType::Info, "Updated camera speed.");
        }
    });

    console.RegisterCommand({
        .Name = "light.intensity",
        .Usage = "light.intensity <ActorName> <value>",
        .Description = "Set a point light intensity.",
        .AutocompleteHandler = [&scene](const std::vector<std::string>&, size_t activeArgIndex) {
            if (activeArgIndex != 0) {
                return std::vector<std::string>{};
            }

            return CollectPointLightActorSuggestions(scene);
        },
        .Handler = [&console, &scene](const std::vector<std::string>& args) {
            if (args.size() != 2) {
                AddUsage(console, "light.intensity <ActorName> <value>");
                return;
            }

            FActor* actor = scene.FindActorByName(args[0]);
            if (actor == nullptr) {
                console.AddMessage(EConsoleMessageType::Error, "Actor not found: " + args[0]);
                return;
            }

            auto* pointLight = actor->GetComponent<CPointLight>();
            if (pointLight == nullptr) {
                console.AddMessage(EConsoleMessageType::Error, "Actor has no point light component: " + actor->Name);
                return;
            }

            float intensity = 0.0f;
            if (!TryParseFloat(args[1], intensity)) {
                console.AddMessage(EConsoleMessageType::Error, "Invalid number: " + args[1]);
                return;
            }

            if (intensity < 0.0f) {
                console.AddMessage(EConsoleMessageType::Error, "Light intensity must be non-negative.");
                return;
            }

            pointLight->Intensity = intensity;
            console.AddMessage(EConsoleMessageType::Info, "Updated light intensity: " + actor->Name);
        }
    });

    console.RegisterCommand({
        .Name = "light.color",
        .Usage = "light.color <ActorName> <r> <g> <b>",
        .Description = "Set a point light RGB color.",
        .AutocompleteHandler = [&scene](const std::vector<std::string>&, size_t activeArgIndex) {
            if (activeArgIndex != 0) {
                return std::vector<std::string>{};
            }

            return CollectPointLightActorSuggestions(scene);
        },
        .Handler = [&console, &scene](const std::vector<std::string>& args) {
            if (args.size() != 4) {
                AddUsage(console, "light.color <ActorName> <r> <g> <b>");
                return;
            }

            FActor* actor = scene.FindActorByName(args[0]);
            if (actor == nullptr) {
                console.AddMessage(EConsoleMessageType::Error, "Actor not found: " + args[0]);
                return;
            }

            auto* pointLight = actor->GetComponent<CPointLight>();
            if (pointLight == nullptr) {
                console.AddMessage(EConsoleMessageType::Error, "Actor has no point light component: " + actor->Name);
                return;
            }

            glm::vec3 color{};
            if (!ParseVec3Arguments(console, args, 1, "light.color <ActorName> <r> <g> <b>", color)) {
                return;
            }

            pointLight->Color = color;
            console.AddMessage(EConsoleMessageType::Info, "Updated light color: " + actor->Name);
        }
    });

    console.RegisterCommand({
        .Name = "render.path",
        .Usage = "render.path <forward|deferred|gpu>",
        .Description = "Switch the active render path.",
        .AutocompleteHandler = [](const std::vector<std::string>&, size_t activeArgIndex) {
            if (activeArgIndex != 0) {
                return std::vector<std::string>{};
            }

            return std::vector<std::string>{"forward", "deferred", "gpu"};
        },
        .Handler = [&console, &editorState](const std::vector<std::string>& args) {
            if (args.size() != 1) {
                AddUsage(console, "render.path <forward|deferred|gpu>");
                return;
            }

            const std::string path = ToLowerCopy(args[0]);
            if (path == "forward") {
                editorState.CurrentRenderPath = ERenderPath::Forward;
                console.AddMessage(EConsoleMessageType::Info, "Render path set to forward.");
                return;
            }

            if (path == "deferred") {
                editorState.CurrentRenderPath = ERenderPath::Deferred;
                console.AddMessage(EConsoleMessageType::Info, "Render path set to deferred.");
                return;
            }

            if (path == "gpu") {
                console.AddMessage(EConsoleMessageType::Warning, "GPU Driven render path is not implemented yet.");
                return;
            }

            AddUsage(console, "render.path <forward|deferred|gpu>");
        }
    });

    console.RegisterCommand({
        .Name = "spawn.light",
        .Usage = "spawn.light <Name> <x> <y> <z> <intensity>",
        .Description = "Spawn a white point light at the requested position.",
        .Handler = [&console, &scene](const std::vector<std::string>& args) {
            if (args.size() != 5) {
                AddUsage(console, "spawn.light <Name> <x> <y> <z> <intensity>");
                return;
            }

            if (scene.FindActorByName(args[0]) != nullptr) {
                console.AddMessage(EConsoleMessageType::Error, "Actor already exists: " + args[0]);
                return;
            }

            glm::vec3 position{};
            if (!ParseVec3Arguments(console, args, 1, "spawn.light <Name> <x> <y> <z> <intensity>", position)) {
                return;
            }

            float intensity = 0.0f;
            if (!TryParseFloat(args[4], intensity)) {
                console.AddMessage(EConsoleMessageType::Error, "Invalid number: " + args[4]);
                return;
            }

            if (intensity < 0.0f) {
                console.AddMessage(EConsoleMessageType::Error, "Light intensity must be non-negative.");
                return;
            }

            FActor* lightActor = scene.CreateActor(args[0]);
            lightActor->Transform.SetPosition(position);
            auto* pointLight = lightActor->AddComponent<CPointLight>();
            pointLight->Color = glm::vec3(1.0f, 1.0f, 1.0f);
            pointLight->Intensity = intensity;

            console.AddMessage(EConsoleMessageType::Info, "Spawned light: " + lightActor->Name);
        }
    });

    console.RegisterCommand({
        .Name = "destroy",
        .Usage = "destroy <ActorName|selected>",
        .Description = "Destroy an actor at the end of the frame.",
        .AutocompleteHandler = [&scene, &camera, &editorState](const std::vector<std::string>&, size_t activeArgIndex) {
            if (activeArgIndex != 0) {
                return std::vector<std::string>{};
            }

            return CollectActorNameSuggestions(scene, editorState, true, false, camera.GetOwner());
        },
        .Handler = [&console, &scene, &camera, &editorState](const std::vector<std::string>& args) {
            if (args.size() != 1) {
                AddUsage(console, "destroy <ActorName|selected>");
                return;
            }

            FActor* actor = ResolveActorReference(scene, editorState, console, args[0]);
            if (actor == nullptr) {
                return;
            }

            if (actor == camera.GetOwner() || actor->Name == "MainCamera") {
                console.AddMessage(EConsoleMessageType::Error, "Destroying MainCamera is not allowed.");
                return;
            }

            if (editorState.SelectedActor == actor) {
                editorState.SelectedActor = nullptr;
            }

            scene.DestroyActor(actor);
            console.AddMessage(EConsoleMessageType::Info, "Queued actor for destruction: " + actor->Name);
        }
    });
}
