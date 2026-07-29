#pragma once

#include <cstdint>
#include <vector>

#include "Assets/MeshLoader.h"
#include "Render/Nanite/Cluster.h"

namespace Tumbler::Nanite {

class ClusterDag {
public:
    void AddMesh(const MeshData& data);

    const std::vector<Cluster>& GetClusters() const { return m_Clusters; }

    // part[triangleIndex] = cluster ID
    const std::vector<int32_t>& GetPart() const { return m_Part; }
    int32_t GetNumTriangles() const { return m_NumTriangles; }

private:
    std::vector<Cluster> m_Clusters;
    std::vector<int32_t> m_Part;
    int32_t m_NumTriangles = 0;
};

} // namespace Tumbler::Nanite
