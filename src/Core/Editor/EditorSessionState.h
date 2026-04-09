#pragma once

#include "Core/Graphics/SceneViewData.h"

class FActor;

struct EditorSessionState {
    FActor* SelectedActor = nullptr;
    ERenderPath CurrentRenderPath = ERenderPath::Forward;
};
