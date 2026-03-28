//
// Created by Icecream_Sarkaz on 2026/1/15.
//

#include "CMeshRenderer.h"
#include "Core/Assets/FMaterialInstance.h"
#include <imgui.h>

CMeshRenderer::CMeshRenderer() = default;
CMeshRenderer::~CMeshRenderer() = default;

void CMeshRenderer::OnDrawUI()
{
    if (ImGui::CollapsingHeader("Mesh Renderer", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (MaterialPtr) {
            bool materialChanged = false;
            
            glm::vec4 baseColor = MaterialPtr->GetBaseColorTint();
            float roughness = MaterialPtr->GetRoughness();
            float metallic = MaterialPtr->GetMetallic();
            float normalStrength = MaterialPtr->GetNormalMapStrength();
            bool twoSided = MaterialPtr->IsTwoSided();
            
            ImGui::Text("Material Parameters");
            if (ImGui::ColorEdit4("Base Color", &baseColor.x)) {
                MaterialPtr->SetVector("BaseColorTint", baseColor);
                materialChanged = true;
            }
            if (ImGui::SliderFloat("Roughness", &roughness, 0.0f, 1.0f)) {
                MaterialPtr->SetFloat("Roughness", roughness);
                materialChanged = true;
            }
            if (ImGui::SliderFloat("Metallic", &metallic, 0.0f, 1.0f)) {
                MaterialPtr->SetFloat("Metallic", metallic);
                materialChanged = true;
            }
            if (ImGui::SliderFloat("Normal Strength", &normalStrength, 0.0f, 2.0f)) {
                MaterialPtr->SetFloat("NormalMapStrength", normalStrength);
                materialChanged = true;
            }
            if (ImGui::Checkbox("Two Sided", &twoSided)) {
                MaterialPtr->SetTwoSided(twoSided);
                materialChanged = true;
            }
            if (materialChanged) {
                MaterialPtr->UpdateUBO();
            }
        } else {
            ImGui::Text("No Material assigned");
        }
    }
}