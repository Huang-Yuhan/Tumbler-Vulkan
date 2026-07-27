#include "MetisGraphWrapper.h"

#include <algorithm>
#include <cassert>

namespace Tumbler::Nanite {

MetisGraphWrapper::MetisGraphWrapper(int32_t numTriangles)
    : m_NumVertices(numTriangles)
    , m_Neighbors(numTriangles) {}

void MetisGraphWrapper::AddEdge(int32_t u, int32_t v, int32_t cost) {
    assert(u >= 0 && u < m_NumVertices);
    assert(v >= 0 && v < m_NumVertices);

    m_Neighbors[u].emplace_back(v, cost);
    m_Neighbors[v].emplace_back(u, cost);
}

MetisGraphWrapper::Result MetisGraphWrapper::Build() {
    Result result;
    result.numVertices = m_NumVertices;

    // Build xadj: sort + dedup each adjacency list, then prefix sum
    result.xadj.resize(m_NumVertices + 1);

    int32_t totalEdges = 0;
    for (int32_t i = 0; i < m_NumVertices; ++i) {
        result.xadj[i] = totalEdges;

        auto& neighbors = m_Neighbors[i];

        // Sort by neighbor id (METIS requires sorted adjncy)
        std::sort(neighbors.begin(), neighbors.end(),
                  [](const auto& a, const auto& b) { return a.first < b.first; });

        // Remove duplicate edges (keep first cost)
        auto last = std::unique(neighbors.begin(), neighbors.end(),
                                [](const auto& a, const auto& b) { return a.first == b.first; });
        neighbors.erase(last, neighbors.end());

        totalEdges += static_cast<int32_t>(neighbors.size());
    }
    result.xadj[m_NumVertices] = totalEdges;

    // Flatten into CSR arrays
    result.adjncy.resize(totalEdges);
    result.adjwgt.resize(totalEdges);

    int32_t idx = 0;
    for (int32_t i = 0; i < m_NumVertices; ++i) {
        for (const auto& [neighbor, cost] : m_Neighbors[i]) {
            result.adjncy[idx] = neighbor;
            result.adjwgt[idx] = cost;
            ++idx;
        }
    }

    return result;
}

} // namespace Tumbler::Nanite
