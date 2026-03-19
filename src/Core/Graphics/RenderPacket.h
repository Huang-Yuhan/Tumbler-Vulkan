#pragma once
#include <glm/glm.hpp>

class FMesh;
class FMaterialInstance;

// 这就是我们的简易版 Render Proxy
struct RenderPacket {
    FMesh* Mesh = nullptr;
    FMaterialInstance* Material = nullptr;
    glm::mat4 TransformMatrix{1.0f};

    // 如果后续需要按材质排序减少 Vulkan 状态切换，可以加上：
    // float DistanceToCamera; 
    // uint32_t SortKey;
};