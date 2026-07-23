#pragma once

#include <cstdint>
#include <vector>
#include "Render/Nanite/NaniteDefinition.h"

namespace Tumbler::Nanite {





struct Cluster {
	// 几何部分
    uint32_t NumTriangles; // 三角形数量
    uint32_t NumVertices;  // 顶点数量
    std::vector<char> VertexData; // 顶点数据
    std::vector<uint32_t> indices;    // 索引数据


};

} // namespace Tumbler::Nanite
