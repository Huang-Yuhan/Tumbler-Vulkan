#include "CPointLight.h"
#include "Core/GameSystem/FActor.h"
#include <imgui.h>

using namespace Tumbler::Math;

void CPointLight::OnDrawUI()
{
    CLightComponent::OnDrawUI();

    if (ImGui::CollapsingHeader("Point Light", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (Owner) {
            Vector3f pos = Owner->Transform.GetPosition();
            ImGui::Text("Position: (%.1f, %.1f, %.1f)", pos.X, pos.Y, pos.Z);
        }
        ImGui::DragFloat("Range", &Range, 1.0f, 0.1f, 500.0f);
    }
}
