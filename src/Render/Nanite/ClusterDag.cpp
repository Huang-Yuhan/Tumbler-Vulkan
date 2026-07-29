#include "ClusterDag.h"
#include "HalfEdge.h"
#include "GraphPartitioner.h"
#include "NaniteDefinition.h"

#include <cassert>
#include <limits>
#include <unordered_map>
#include <glm/glm.hpp>

namespace Tumbler::Nanite {

void ClusterDag::AddMesh(const MeshData& data) {
    auto& verts   = data.vertices;
    auto& indices = data.indices;

    const uint32_t numEdges    = static_cast<uint32_t>(indices.size());
    const int32_t  numTriangles = static_cast<int32_t>(indices.size() / 3);

    // ---- 1. Build half-edge hash table ----
    HalfEdgeHashTable edgeHash;

    auto GetPos = [&](size_t idx) -> glm::vec3 {
        return verts[indices[idx]].pos;
    };

    for (uint32_t i = 0; i < numEdges; ++i) {
        edgeHash.AddEdge(i, GetPos);
    }

    // ---- 2. Build Direct adjacency array ----
    std::vector<int32_t> directAdj;
    edgeHash.BuildDirectAdjacency(directAdj, numEdges, GetPos);

    // ---- 3. Build DisjointSet (union triangles that share an edge) ----
    DisjointSet disjointSet(numTriangles);

    HalfEdgeHashTable::ForAllPairs(directAdj, numEdges,
        [&](uint32_t e0, uint32_t e1) {
            int32_t triA = static_cast<int32_t>(e0 / 3);
            int32_t triB = static_cast<int32_t>(e1 / 3);
            if (triA != triB) {
                disjointSet.Union(triA, triB);
            }
        });

    // ---- 4. Compute vertex bounds ----
    glm::vec3 boundsMin( std::numeric_limits<float>::max());
    glm::vec3 boundsMax(-std::numeric_limits<float>::max());
    for (const auto& v : verts) {
        boundsMin = glm::min(boundsMin, v.pos);
        boundsMax = glm::max(boundsMax, v.pos);
    }
    // Expand slightly to avoid zero extent
    glm::vec3 extent = boundsMax - boundsMin;
    if (extent.x < 1e-6f) { boundsMin.x -= 0.5f; boundsMax.x += 0.5f; }
    if (extent.y < 1e-6f) { boundsMin.y -= 0.5f; boundsMax.y += 0.5f; }
    if (extent.z < 1e-6f) { boundsMin.z -= 0.5f; boundsMax.z += 0.5f; }

    // ---- 5. Create GraphPartitioner ----
    GraphPartitioner partitioner(
        numTriangles,
        static_cast<int32_t>(kClusterTriangleCount) - 4,
        static_cast<int32_t>(kClusterTriangleCount));

    auto GetCenter = [&](uint32_t triIdx) -> glm::vec3 {
        glm::vec3 c;
        c  = verts[indices[triIdx * 3 + 0]].pos;
        c += verts[indices[triIdx * 3 + 1]].pos;
        c += verts[indices[triIdx * 3 + 2]].pos;
        return c * (1.0f / 3.0f);
    };

    std::vector<int32_t> emptyMaterialIndexes; // no material grouping for now
    partitioner.BuildLocalityLinks(disjointSet, boundsMin, boundsMax,
                                   emptyMaterialIndexes, GetCenter);

    // ---- 6. Build top-level graph ----
    auto* graph = partitioner.NewGraph(numTriangles * 3);

    for (int32_t i = 0; i < numTriangles; ++i) {
        graph->AdjacencyOffset[i] = static_cast<idx_t>(graph->Adjacency.size());

        uint32_t triIndex = partitioner.Indexes[i];

        // Add adjacency edges (high cost = 260 → prioritize topological continuity)
        for (int k = 0; k < 3; ++k) {
            uint32_t edgeIndex = triIndex * 3 + k;
            HalfEdgeHashTable::ForEachAdjacency(directAdj, edgeIndex,
                [&](uint32_t /*e0*/, uint32_t e1) {
                    uint32_t adjTri = e1 / 3;
                    partitioner.AddAdjacency(graph, adjTri, 4 * 65);
                });
        }

        // Add locality links (low cost = 1 → soft spatial constraint)
        partitioner.AddLocalityLinks(graph, triIndex, 1);
    }
    graph->AdjacencyOffset[numTriangles] = static_cast<idx_t>(graph->Adjacency.size());

    // ---- 7. Partition ----
    partitioner.PartitionStrict(graph, /*bThreaded=*/false);

    // ---- 8. Extract clusters from partitioner output ----
    const auto& ranges   = partitioner.Ranges;
    const auto& sortedIdx = partitioner.Indexes;   // sortedIdx[pos] = global triangle idx
    const auto& sortedTo  = partitioner.SortedTo;   // sortedTo[triIdx] = position

    // Build sortedTriangles (inverse of sortedTo): sortedTriangles[pos] = triangle idx at pos
    std::vector<int32_t> sortedTriangles(numTriangles);
    for (int32_t t = 0; t < numTriangles; ++t) {
        sortedTriangles[sortedTo[t]] = t;
    }

    // Store partition for visualization: part[triIdx] = cluster ID
    m_Part.assign(numTriangles, 0);
    m_NumTriangles = numTriangles;

    m_Clusters.clear();
    m_Clusters.reserve(ranges.size());

    for (int32_t c = 0; c < static_cast<int32_t>(ranges.size()); ++c) {
        const auto& range = ranges[c];
        Cluster cluster;

        std::unordered_map<uint32_t, uint32_t> oldToNew;
        std::vector<uint32_t> localIndices;

        for (int32_t pos = range.start; pos < range.end; ++pos) {
            int32_t tri = sortedTriangles[pos];  // global triangle index
            m_Part[tri] = c;  // fill part for visualization

            for (int32_t k = 0; k < 3; ++k) {
                uint32_t oldIdx = indices[tri * 3 + k];
                auto [it, inserted] = oldToNew.try_emplace(oldIdx,
                    static_cast<uint32_t>(oldToNew.size()));
                localIndices.push_back(it->second);
            }
        }

        uint32_t actualTriangles = static_cast<uint32_t>(localIndices.size() / 3);
        cluster.NumTriangles = actualTriangles;
        cluster.NumVertices  = static_cast<uint32_t>(oldToNew.size());

        // Pad with degenerate triangles to reach kClusterTriangleCount
        if (actualTriangles < kClusterTriangleCount) {
            uint32_t padIdx = localIndices.empty() ? 0 : localIndices[0];
            while (localIndices.size() < kClusterTriangleCount * 3) {
                localIndices.push_back(padIdx);
                localIndices.push_back(padIdx);
                localIndices.push_back(padIdx);
            }
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
