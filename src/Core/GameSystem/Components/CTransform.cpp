#include "CTransform.h"
#include <algorithm>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

void CTransform::SetParent(CTransform* newParent, bool bStayWorldPos)
{
    if (Parent == newParent) return;

    // Capture current world matrix if staying in place
    glm::mat4 currentWorldMatrix = GetLocalToWorldMatrix();

    // Remove from old parent
    if (Parent)
    {
        Parent->RemoveChild(this);
    }

    // Set new parent
    Parent = newParent;
    if (Parent)
    {
        Parent->AddChild(this);
    }

    // Keep world position?
    if (bStayWorldPos)
    {
        glm::mat4 newLocalMatrix = currentWorldMatrix;
        if (Parent)
        {
            glm::mat4 parentWorldInv = glm::inverse(Parent->GetLocalToWorldMatrix());
            newLocalMatrix = parentWorldInv * currentWorldMatrix;
        }

        glm::vec3 scale;
        glm::quat rotation;
        glm::vec3 translation;
        glm::vec3 skew;
        glm::vec4 perspective;

        if (glm::decompose(newLocalMatrix, scale, rotation, translation, skew, perspective))
        {
            Position = translation;
            Rotation = FQuaternion(rotation);
            Scale = scale;
        }
    }
    
    bIsLocalDirty = true;
    MarkWorldDirty();
}

void CTransform::AddChild(CTransform* child)
{
    if (child && std::find(Children.begin(), Children.end(), child) == Children.end())
    {
        Children.push_back(child);
    }
}

void CTransform::RemoveChild(CTransform* child)
{
    auto it = std::find(Children.begin(), Children.end(), child);
    if (it != Children.end())
    {
        Children.erase(it);
    }
}

void CTransform::MarkWorldDirty()
{
    if (!bIsWorldDirty)
    {
        bIsWorldDirty = true;
        for (auto* child : Children)
        {
            child->MarkWorldDirty();
        }
    }
}

const glm::mat4& CTransform::GetLocalMatrix() const
{
    if (bIsLocalDirty)
    {
        glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), Position);
        glm::mat4 rotationMatrix = Rotation.ToMatrix();
        glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), Scale);
        CachedLocalMatrix = translationMatrix * rotationMatrix * scaleMatrix;
        bIsLocalDirty = false;
    }
    return CachedLocalMatrix;
}

const glm::mat4& CTransform::GetLocalToWorldMatrix() const
{
    if (bIsWorldDirty)
    {
        if (Parent)
        {
            CachedWorldMatrix = Parent->GetLocalToWorldMatrix() * GetLocalMatrix();
        }
        else
        {
            CachedWorldMatrix = GetLocalMatrix();
        }
        bIsWorldDirty = false;
    }
    return CachedWorldMatrix;
}
