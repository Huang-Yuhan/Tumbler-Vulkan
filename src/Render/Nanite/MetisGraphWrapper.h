#pragma once

#include <cstdint>
#include <vector>

namespace Tumbler::Nanite {

// Builds a METIS-compatible CSR graph from mesh triangle adjacency.
//
// Nodes = triangles, Edges = shared physical edge between two triangles.
// Each edge is undirected with an explicit cost.
//
// Usage:
//   MetisGraphWrapper builder(numTriangles);
//   for each (a, b) adjacent: builder.AddEdge(a, b, cost);
//   auto metisGraph = builder.Build();

class MetisGraphWrapper {
public:
    // Immutable METIS CSR data, produced by Build().
    struct Result {
        int32_t                numVertices;
        std::vector<int32_t>   xadj;    // CSR offset array
        std::vector<int32_t>   adjncy;  // CSR adjacency list
        std::vector<int32_t>   adjwgt;  // CSR edge weights
    };

    explicit MetisGraphWrapper(int32_t numTriangles);
    void AddEdge(int32_t u, int32_t v, int32_t cost);
    Result Build();

private:
    int32_t m_NumVertices;
    std::vector<std::vector<std::pair<int32_t, int32_t>>> m_Neighbors;
};

} // namespace Tumbler::Nanite
