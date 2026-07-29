#include "ClusterDag.h"
#include "HalfEdge.h"
#include "GraphPartitioner.h"
#include "MetisGraphWrapper.h"

#include <unordered_map>

namespace Tumbler::Nanite {

void ClusterDag::AddMesh(const MeshData& data) {
    auto& verts   = data.vertices;
    auto& indices = data.indices;

    // ---- 1. Build half-edge adjacency → triangle pairs ----
    HalfEdgeHashTable hashTable;
    std::vector<std::pair<int32_t, int32_t>> triangleEdges;

    auto GetPos = [&](size_t idx) { return verts[indices[idx]].pos; };

    for (size_t i = 0; i < indices.size(); ++i) {
        hashTable.AddEdge(static_cast<uint32_t>(i), GetPos);
    }

    for (size_t i = 0; i < indices.size(); ++i) {
        hashTable.ForAllMatching(static_cast<uint32_t>(i), GetPos,
                                 [&](uint32_t a, uint32_t b) {
            int32_t triA = static_cast<int32_t>(a / 3);
            int32_t triB = static_cast<int32_t>(b / 3);
            if (triA != triB) {
                triangleEdges.emplace_back(triA, triB);
            }
        });
    }

    // ---- 2. Build triangle adjacency graph ----
    int32_t numTriangles = static_cast<int32_t>(indices.size() / 3);
    MetisGraphWrapper graphBuilder(numTriangles);

    for (auto [triA, triB] : triangleEdges) {
        // TODO: cost based on shared-edge length
        graphBuilder.AddEdge(triA, triB, 1);
    }

    auto metisGraph = graphBuilder.Build();

    // ---- 3. Partition ----
    GraphPartitioner partitioner;
    auto partResult = partitioner.Partition(metisGraph);
    if (!partResult) {
        // TODO: propagate error
        return;
    }

    // Store partition for visualization
    m_Part         = partResult->part;
    m_NumTriangles = numTriangles;

    // ---- 4. Build sortedTriangles (inverse of sortedTo) ----
    std::vector<int32_t> sortedTriangles(numTriangles);
    for (int32_t t = 0; t < numTriangles; ++t) {
        sortedTriangles[partResult->sortedTo[t]] = t;
    }

    // ---- 5. Extract clusters ----
    m_Clusters.clear();
    m_Clusters.reserve(partResult->clusters.size());

    for (const auto& range : partResult->clusters) {
        Cluster cluster;

        std::unordered_map<uint32_t, uint32_t> oldToNew;
        std::vector<uint32_t> localIndices;

        for (int32_t pos = range.start; pos < range.end; ++pos) {
            int32_t tri = sortedTriangles[pos];
            for (int32_t k = 0; k < 3; ++k) {
                uint32_t oldIdx = indices[tri * 3 + k];
                auto [it, inserted] = oldToNew.try_emplace(oldIdx,
                    static_cast<uint32_t>(oldToNew.size()));
                localIndices.push_back(it->second);
            }
        }

        uint32_t actualTriangles = static_cast<uint32_t>(localIndices.size() / 3);
        cluster.NumTriangles = kClusterTriangleCount;
        cluster.NumVertices  = static_cast<uint32_t>(oldToNew.size());

        // Pad with degenerate triangles if fewer than kClusterTriangleCount
        uint32_t padIdx = localIndices.empty() ? 0 : localIndices[0];
        while (localIndices.size() < kClusterTriangleCount * 3) {
            localIndices.push_back(padIdx);
            localIndices.push_back(padIdx);
            localIndices.push_back(padIdx);
        }

        cluster.VertexData.resize(cluster.NumVertices * sizeof(Vertex));
        auto* dstVerts = reinterpret_cast<Vertex*>(cluster.VertexData.data());
        for (const auto& [oldIdx, newIdx] : oldToNew) {
            dstVerts[newIdx] = verts[oldIdx];
        }

        cluster.indices = std::move(localIndices);
        m_Clusters.push_back(std::move(cluster));
    }
}

} // namespace Tumbler::Nanite
