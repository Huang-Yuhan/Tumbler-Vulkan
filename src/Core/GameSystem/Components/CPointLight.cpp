#include "CPointLight.h"
#include "Core/GameSystem/FActor.h"
#include <imgui.h>

void CPointLight::OnDrawUI()
{
    CLightComponent::OnDrawUI();

    if (ImGui::CollapsingHeader("Point Light", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (Owner) {
            glm::vec3 pos = Owner->Transform.GetPosition();
            ImGui::Text("Position: (%.1f, %.1f, %.1f)", pos.x, pos.y, pos.z);
        }
        ImGui::DragFloat("Range", &Range, 1.0f, 0.1f, 500.0f);
    }
}
