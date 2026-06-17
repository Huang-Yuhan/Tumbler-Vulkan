#include "CTransform.h"
#include <algorithm>
#include <imgui.h>

namespace Tumbler {

using namespace Tumbler::Math;

void CTransform::SetParent(CTransform* newParent, bool bStayWorldPos) {
    if (Parent == newParent)
        return;

    // Capture current world matrix if staying in place
    Matrix4f currentWorldMatrix = GetLocalToWorldMatrix();

    // Remove from old parent
    if (Parent) {
        Parent->RemoveChild(this);
    }

    // Set new parent
    Parent = newParent;
    if (Parent) {
        Parent->AddChild(this);
    }

    // Keep world position?
    if (bStayWorldPos) {
        Matrix4f newLocalMatrix = currentWorldMatrix;
        if (Parent) {
            Matrix4f parentWorldInv = Inverse(Parent->GetLocalToWorldMatrix());
            newLocalMatrix = parentWorldInv * currentWorldMatrix;
        }

        Vector3f scale;
        Quaternionf rotation;
        Vector3f translation;

        if (Decompose(newLocalMatrix, translation, rotation, scale)) {
            Position = translation;
            Rotation = rotation;
            Scale = scale;
        }
    }

    bIsLocalDirty = true;
    MarkWorldDirty();
}

void CTransform::AddChild(CTransform* child) {
    if (child && std::find(Children.begin(), Children.end(), child) == Children.end()) {
        Children.push_back(child);
    }
}

void CTransform::RemoveChild(CTransform* child) {
    auto it = std::find(Children.begin(), Children.end(), child);
    if (it != Children.end()) {
        Children.erase(it);
    }
}

void CTransform::MarkWorldDirty() {
    if (!bIsWorldDirty) {
        bIsWorldDirty = true;
        for (auto* child : Children) {
            child->MarkWorldDirty();
        }
    }
}

const Matrix4f& CTransform::GetLocalMatrix() const {
    if (bIsLocalDirty) {
        Matrix4f translationMatrix = MakeTranslation(Position);
        Matrix4f rotationMatrix = Rotation.ToMatrix();
        Matrix4f scaleMatrix = MakeScale(Scale);
        CachedLocalMatrix = translationMatrix * rotationMatrix * scaleMatrix;
        bIsLocalDirty = false;
    }
    return CachedLocalMatrix;
}

const Matrix4f& CTransform::GetLocalToWorldMatrix() const {
    if (bIsWorldDirty) {
        if (Parent) {
            CachedWorldMatrix = Parent->GetLocalToWorldMatrix() * GetLocalMatrix();
        } else {
            CachedWorldMatrix = GetLocalMatrix();
        }
        bIsWorldDirty = false;
    }
    return CachedWorldMatrix;
}

void CTransform::OnDrawUI() {
    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        Vector3f pos = GetPosition();
        if (ImGui::DragFloat3("Position", &pos.X, 0.1f)) {
            SetPosition(pos);
        }

        Vector3f rot = GetEulerAngles();
        if (ImGui::DragFloat3("Rotation", &rot.X, 1.0f)) {
            SetRotation(rot);
        }

        Vector3f scale = GetScale();
        if (ImGui::DragFloat3("Scale", &scale.X, 0.1f)) {
            SetScale(scale);
        }
    }
}

} // namespace Tumbler
