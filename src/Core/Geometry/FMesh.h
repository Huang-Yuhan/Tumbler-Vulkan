#pragma once

#include <cstdint>
#include <vector>


// 枚举保持在头文件
enum class EVertexAttribute : uint32_t {
    None      = 0,
    Position  = 1 << 0,
    Normal    = 1 << 1,
    Tangent   = 1 << 2,
    Color     = 1 << 3,
    UV0       = 1 << 4,
    UV1       = 1 << 5
};

struct FVertexElement
{
    EVertexAttribute Attribute;
    uint32_t Offset;
    uint32_t FormatSize;
};

struct FVertexLayout
{
    uint32_t Stride = 0; // C++11 可以在这里直接初始化
    std::vector<FVertexElement> Elements;

    // 只保留声明
    void AddElement(EVertexAttribute attribute, uint32_t formatSize);
};

class FMesh
{
private:
    std::vector<uint8_t> RawVertexData;
    std::vector<uint32_t> Indices;
    FVertexLayout VertexLayout;

public:
    // Getters 写在头文件里没问题，方便内联
    [[nodiscard]] const std::vector<uint8_t>& GetRawVertexData() const { return RawVertexData; }
    [[nodiscard]] const std::vector<uint32_t>& GetIndices() const { return Indices; }
    [[nodiscard]] const FVertexLayout& GetVertexLayout() const { return VertexLayout; }

    // [重点] 复杂的工厂函数只保留声明
    static FMesh CreatePlane(float width, float height, uint32_t subdivisionsWidth, uint32_t subdivisionsHeight);
};
