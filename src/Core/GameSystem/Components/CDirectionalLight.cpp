#include "CDirectionalLight.h"
#include "Core/GameSystem/FActor.h"
#include <imgui.h>

void CDirectionalLight::OnDrawUI()
{
    CLightComponent::OnDrawUI();

    if (ImGui::CollapsingHeader("Directional Light", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (Owner) {
            glm::vec3 dir = Owner->Transform.GetForwardVector();
            ImGui::Text("Direction: (%.2f, %.2f, %.2f)", dir.x, dir.y, dir.z);
        }
    }
}
