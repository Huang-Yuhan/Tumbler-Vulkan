#pragma once

#include "Core/Graphics/SceneViewData.h"

class FActor;

struct EditorSessionState {
    FActor* SelectedActor = nullptr;
    bool ShowDebugPanel = true;
};

struct RenderSettings {
    ERenderPath CurrentRenderPath = ERenderPath::Forward;
};
