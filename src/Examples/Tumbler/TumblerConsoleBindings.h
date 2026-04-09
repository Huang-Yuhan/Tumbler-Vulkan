#pragma once

class RuntimeConsole;
class FScene;
class CFirstPersonCamera;
struct EditorSessionState;

void RegisterTumblerConsoleCommands(
    RuntimeConsole& console,
    FScene& scene,
    CFirstPersonCamera& camera,
    EditorSessionState& editorState);
