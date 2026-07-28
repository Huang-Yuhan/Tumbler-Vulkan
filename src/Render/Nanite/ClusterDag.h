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

private:
    std::vector<Cluster> m_Clusters;
};

} // namespace Tumbler::Nanite
