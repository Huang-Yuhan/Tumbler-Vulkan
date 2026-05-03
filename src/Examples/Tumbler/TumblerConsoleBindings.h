#pragma once

class RuntimeConsole;
class FScene;
class CFirstPersonCamera;
struct EditorSessionState;
struct RenderSettings;

void RegisterTumblerConsoleCommands(
    RuntimeConsole& console,
    FScene& scene,
    CFirstPersonCamera& camera,
    EditorSessionState& editorState,
    RenderSettings& renderSettings);
