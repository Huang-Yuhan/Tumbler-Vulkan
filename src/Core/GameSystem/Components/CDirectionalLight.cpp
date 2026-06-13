#include "CDirectionalLight.h"
#include "Core/GameSystem/FActor.h"
#include <imgui.h>

using namespace Tumbler::Math;

void CDirectionalLight::OnDrawUI()
{
    CLightComponent::OnDrawUI();

    if (ImGui::CollapsingHeader("Directional Light", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (Owner) {
            Vector3f dir = Owner->Transform.GetForwardVector();
            ImGui::Text("Direction: (%.2f, %.2f, %.2f)", dir.X, dir.Y, dir.Z);
        }
    }
}
