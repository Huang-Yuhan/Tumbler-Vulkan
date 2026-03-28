#include "CPointLight.h"
#include <imgui.h>

void CPointLight::OnDrawUI()
{
    if (ImGui::CollapsingHeader("Point Light", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::ColorEdit3("Light Color", &Color.x);
        ImGui::DragFloat("Intensity", &Intensity, 1.0f, 0.0f, 1000.0f);
    }
}
