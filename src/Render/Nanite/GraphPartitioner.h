#pragma once

#include "Core/Utils/Range.h"

#include <algorithm>
#include <cstdint>
#include <glm/glm.hpp>
#include <limits>
#include <vector>

#include <metis.h>

namespace Tumbler::Nanite {

using Range = Tumbler::Utils::Range;

// Simple disjoint-set / union-find for tracking connected components.
class DisjointSet {
public:
    explicit DisjointSet(uint32_t n) : m_Parent(n) {
        for (uint32_t i = 0; i < n; ++i) m_Parent[i] = i;
    }

    uint32_t Find(uint32_t x) const {
        while (m_Parent[x] != x) x = m_Parent[x];
        return x;
    }

    void Union(uint32_t a, uint32_t b) {
        uint32_t ra = Find(a), rb = Find(b);
        if (ra != rb) m_Parent[ra] = rb;
    }

    uint32_t operator[](uint32_t i) const { return Find(i); }

private:
    mutable std::vector<uint32_t> m_Parent;
};

// Graph partitioner that produces compact clusters using METIS recursive bisection.
//
// Key features over basic METIS usage:
//  - Spatial locality links: weak edges between nearby triangles from DIFFERENT
//    connected components → forces spatially compact clusters.
//  - Imbalanced bisection weights: ensures each split converges to the target
//    cluster size without producing degenerate fragments.
//  - Tight cluster size range: [MaxPartitionSize-4, MaxPartitionSize].
//
// Typical usage:
//   FGraphPartitioner p(numTriangles, clusterSize - 4, clusterSize);
//   p.BuildLocalityLinks(disjointSet, bounds, materialIndexes, getCenter);
//   auto* graph = p.NewGraph(numTriangles * 3);
//   // for each triangle: add adjacency edges + locality links
//   p.PartitionStrict(graph, /*bThreaded=*/false);
//   // p.Ranges, p.Indexes, p.SortedTo are now populated
class GraphPartitioner {
public:
    struct FGraphData {
        int32_t              Offset = 0;
        int32_t              Num    = 0;
        std::vector<idx_t>   Adjacency;
        std::vector<idx_t>   AdjacencyCost;
        std::vector<idx_t>   AdjacencyOffset;
    };

    std::vector<Range>    Ranges;
    std::vector<uint32_t> Indexes;    // Indexes[pos] = global triangle index
    std::vector<uint32_t> SortedTo;   // SortedTo[triIdx] = position in Indexes

    GraphPartitioner(uint32_t numElements,
                     int32_t  minPartitionSize,
                     int32_t  maxPartitionSize);

    // Allocate a top-level graph with pre-reserved adjacency slots.
    FGraphData* NewGraph(uint32_t numAdjacency) const;

    // Inline helpers — remap AdjIndex through SortedTo before adding.
    void AddAdjacency(FGraphData* graph, uint32_t adjIndex, idx_t cost);
    void AddLocalityLinks(FGraphData* graph, uint32_t index, idx_t cost);

    // Build spatial locality links: Morton-code sort + 5 nearest neighbours
    // from different DisjointSet islands / material groups.
    template <typename FGetCenter>
    void BuildLocalityLinks(DisjointSet&               disjointSet,
                            const glm::vec3&           boundsMin,
                            const glm::vec3&           boundsMax,
                            const std::vector<int32_t>& groupIndexes,
                            FGetCenter&&               getCenter);

    // Recursive bisection producing strictly bounded leaf partitions.
    void PartitionStrict(FGraphData* graph, bool bThreaded);

private:
    void BisectGraph(FGraphData* graph, FGraphData* childGraphs[2]);
    void RecursiveBisectGraph(FGraphData* graph);

    uint32_t m_NumElements;
    int32_t  m_MinPartitionSize;
    int32_t  m_MaxPartitionSize;

    int32_t            m_NumPartitions = 0;
    std::vector<idx_t> m_PartitionIDs;
    std::vector<int32_t> m_SwappedWith;

    // Multi-map: LocalityLinks[triangleA] = { triangleB, ... }
    std::vector<std::vector<uint32_t>> m_LocalityLinks;  // adjacency list per element
    std::vector<std::pair<uint32_t, uint32_t>> m_LocalityPairs; // flat storage
};

// ---- Inline implementations ----

inline GraphPartitioner::GraphPartitioner(uint32_t numElements,
                                          int32_t  minPartitionSize,
                                          int32_t  maxPartitionSize)
    : m_NumElements(numElements)
    , m_MinPartitionSize(minPartitionSize)
    , m_MaxPartitionSize(maxPartitionSize)
{
    Indexes.resize(numElements);
    for (uint32_t i = 0; i < numElements; ++i)
        Indexes[i] = i;
    m_LocalityLinks.resize(numElements);
}

inline GraphPartitioner::FGraphData* GraphPartitioner::NewGraph(uint32_t numAdjacency) const {
    numAdjacency += static_cast<uint32_t>(m_LocalityPairs.size());
    auto* graph         = new FGraphData;
    graph->Offset       = 0;
    graph->Num          = static_cast<int32_t>(m_NumElements);
    graph->Adjacency.reserve(numAdjacency);
    graph->AdjacencyCost.reserve(numAdjacency);
    graph->AdjacencyOffset.resize(m_NumElements + 1);
    return graph;
}

inline void GraphPartitioner::AddAdjacency(FGraphData* graph, uint32_t adjIndex, idx_t cost) {
    graph->Adjacency.push_back(static_cast<idx_t>(SortedTo[adjIndex]));
    graph->AdjacencyCost.push_back(cost);
}

inline void GraphPartitioner::AddLocalityLinks(FGraphData* graph, uint32_t index, idx_t cost) {
    for (uint32_t adjIndex : m_LocalityLinks[index]) {
        graph->Adjacency.push_back(static_cast<idx_t>(SortedTo[adjIndex]));
        graph->AdjacencyCost.push_back(cost);
    }
}

// Morton code for 3D spatial ordering (10 bits per axis → 30 bits).
inline uint32_t MortonCode3(uint32_t x, uint32_t y, uint32_t z) {
    auto spread = [](uint32_t v) {
        v = (v | (v << 16)) & 0x030000FF;
        v = (v | (v <<  8)) & 0x0300F00F;
        v = (v | (v <<  4)) & 0x030C30C3;
        v = (v | (v <<  2)) & 0x09249249;
        return v;
    };
    return spread(x) | (spread(y) << 1) | (spread(z) << 2);
}

template <typename FGetCenter>
void GraphPartitioner::BuildLocalityLinks(
    DisjointSet&               disjointSet,
    const glm::vec3&           boundsMin,
    const glm::vec3&           boundsMax,
    const std::vector<int32_t>& groupIndexes,
    FGetCenter&&               getCenter)
{
    const bool bUseGroups = !groupIndexes.empty();

    // ---- 1. Sort triangles by Morton code ----
    std::vector<uint32_t> mortonCodes(m_NumElements);
    glm::vec3 extent = boundsMax - boundsMin;
    float invMaxExtent = 1.0f / glm::max(glm::max(extent.x, extent.y), extent.z);

    for (uint32_t i = 0; i < m_NumElements; ++i) {
        glm::vec3 c = getCenter(i);
        glm::vec3 local = (c - boundsMin) * invMaxExtent;
        uint32_t mx = static_cast<uint32_t>(glm::clamp(local.x, 0.0f, 1.0f) * 1023.0f);
        uint32_t my = static_cast<uint32_t>(glm::clamp(local.y, 0.0f, 1.0f) * 1023.0f);
        uint32_t mz = static_cast<uint32_t>(glm::clamp(local.z, 0.0f, 1.0f) * 1023.0f);
        mortonCodes[i] = MortonCode3(mx, my, mz);
    }

    SortedTo.resize(m_NumElements);
    {
        std::vector<uint32_t> sortIndices(m_NumElements);
        for (uint32_t i = 0; i < m_NumElements; ++i) sortIndices[i] = i;
        std::sort(sortIndices.begin(), sortIndices.end(),
            [&](uint32_t a, uint32_t b) { return mortonCodes[a] < mortonCodes[b]; });

        std::vector<uint32_t> oldIndexes = std::move(Indexes);
        Indexes.resize(m_NumElements);
        for (uint32_t i = 0; i < m_NumElements; ++i) {
            Indexes[i] = oldIndexes[sortIndices[i]];
            SortedTo[Indexes[i]] = i;
        }
    }

    // ---- 2. Island runs (for skipping same-component ranges) ----
    struct IslandRun { uint32_t begin, end; };
    std::vector<IslandRun> islandRuns(m_NumElements);
    {
        uint32_t runIsland  = ~0u;
        uint32_t runFirst   = 0;
        for (uint32_t i = 0; i < m_NumElements; ++i) {
            uint32_t island = disjointSet[Indexes[i]];
            if (runIsland != island) {
                for (uint32_t j = runFirst; j < i; ++j) islandRuns[j].end = i;
                runIsland = island;
                runFirst  = i;
            }
            islandRuns[i].begin = runFirst;
        }
        for (uint32_t j = runFirst; j < m_NumElements; ++j)
            islandRuns[j].end = m_NumElements;
    }

    // ---- 3. Find nearest neighbours from different islands ----
    for (uint32_t i = 0; i < m_NumElements; ++i) {
        uint32_t index   = Indexes[i];
        uint32_t island  = disjointSet[index];
        int32_t  groupId = bUseGroups ? groupIndexes[index] : 0;

        uint32_t runLen = islandRuns[i].end - islandRuns[i].begin;
        if (runLen >= 128) continue; // already a big connected component

        glm::vec3 center = getCenter(index);

        constexpr uint32_t kMaxLinks = 5;
        uint32_t closestIdx[kMaxLinks];
        float    closestD2[kMaxLinks];
        for (uint32_t k = 0; k < kMaxLinks; ++k) {
            closestIdx[k] = ~0u;
            closestD2[k] = std::numeric_limits<float>::max();
        }

        for (int dir = 0; dir < 2; ++dir) {
            uint32_t limit = dir ? m_NumElements - 1 : 0;
            int32_t  step  = dir ? 1 : -1;

            uint32_t adj = i;
            for (int32_t iter = 0; iter < 16; ++iter) {
                if (adj == limit) break;
                adj = static_cast<uint32_t>(static_cast<int32_t>(adj) + step);

                uint32_t adjIndex  = Indexes[adj];
                uint32_t adjIsland = disjointSet[adjIndex];
                int32_t  adjGroup  = bUseGroups ? groupIndexes[adjIndex] : 0;

                if (island == adjIsland || groupId != adjGroup) {
                    // Skip entire island run
                    if (dir) adj = islandRuns[adj].end - 1;
                    else     adj = islandRuns[adj].begin;
                } else {
                    glm::vec3 diff = center - getCenter(adjIndex);
                    float d2 = glm::dot(diff, diff);
                    for (int k = 0; k < static_cast<int>(kMaxLinks); ++k) {
                        if (d2 < closestD2[k]) {
                            std::swap(adjIndex, closestIdx[k]);
                            std::swap(d2, closestD2[k]);
                        }
                    }
                }
            }
        }

        for (uint32_t k = 0; k < kMaxLinks; ++k) {
            if (closestIdx[k] != ~0u) {
                m_LocalityLinks[index].push_back(closestIdx[k]);
                m_LocalityLinks[closestIdx[k]].push_back(index);
            }
        }
    }

    // Deduplicate locality links
    for (auto& vec : m_LocalityLinks) {
        std::sort(vec.begin(), vec.end());
        vec.erase(std::unique(vec.begin(), vec.end()), vec.end());
    }

    // Flatten for NewGraph size estimation
    for (uint32_t i = 0; i < m_NumElements; ++i) {
        for (uint32_t adj : m_LocalityLinks[i]) {
            if (i < adj) m_LocalityPairs.emplace_back(i, adj);
        }
    }
}

} // namespace Tumbler::Nanite
