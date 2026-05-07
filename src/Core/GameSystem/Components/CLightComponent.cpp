#include "CLightComponent.h"
#include <imgui.h>

void CLightComponent::OnDrawUI()
{
    if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::ColorEdit3("Color", &Color.x);
        ImGui::DragFloat("Intensity", &Intensity, 1.0f, 0.0f, 1000.0f);
    }
}
