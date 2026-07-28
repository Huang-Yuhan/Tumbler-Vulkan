#include "GraphPartitioner.h"

#include <metis.h>
#include <numeric>

namespace Tumbler::Nanite {

// ---- Private ----

std::expected<void, PartitionError>
GraphPartitioner::Bisect(const MetisGraphWrapper::Result& graph,
                         int32_t first,
                         const PartitionSettings& settings,
                         std::vector<int32_t>& sortedTo,
                         std::vector<int32_t>& sortedTriangles,
                         std::vector<int32_t>& part,
                         std::vector<Range>& clusters) {
    const int32_t N = graph.numVertices;

    // ---- 1. Leaf: this subgraph is already a cluster ----
    if (N <= settings.maxClusterSize) {
        int32_t clusterId = static_cast<int32_t>(clusters.size());
        for (int32_t i = 0; i < N; ++i) {
            int32_t globalIdx = sortedTriangles[first + i];
            part[globalIdx] = clusterId;
        }
        clusters.push_back(Range{first, first + N});
        return {};
    }

    // ---- 2. Bisect with METIS_PartGraphRecursive(nparts=2) ----
    idx_t ncon = 1;
    idx_t nparts = 2;
    idx_t edgecut = 0;
    std::vector<idx_t> localPart(N);

    idx_t options[METIS_NOPTIONS];
    METIS_SetDefaultOptions(options);
    // TODO: tune these for triangle mesh quality
    //   options[METIS_OPTION_CTYPE]  = METIS_CTYPE_SHEM;
    //   options[METIS_OPTION_IPTYPE] = METIS_IPTYPE_EDGE;
    //   options[METIS_OPTION_RTYPE]  = METIS_RTYPE_SEP2SIDED;
    //   options[METIS_OPTION_MINCONN] = 1;
    //   options[METIS_OPTION_CONTIG]  = 1;

    int ret = METIS_PartGraphRecursive(
        const_cast<idx_t*>(&N),      // nvtxs
        &ncon,                         // ncon
        const_cast<idx_t*>(graph.xadj.data()),     // xadj
        const_cast<idx_t*>(graph.adjncy.data()),   // adjncy
        nullptr,                                   // vwgt
        nullptr,                                   // vsize
        const_cast<idx_t*>(graph.adjwgt.data()),   // adjwgt
        &nparts,                                   // nparts
        nullptr,                                   // tpwgts (equal weight)
        nullptr,                                   // ubvec (default unbalance)
        options,                                   // options
        &edgecut,                                  // edgecut
        localPart.data());                         // part

    if (ret != METIS_OK) {
        switch (ret) {
        case METIS_ERROR_INPUT:  return std::unexpected(PartitionError::InvalidInput);
        case METIS_ERROR_MEMORY: return std::unexpected(PartitionError::OutOfMemory);
        default:                 return std::unexpected(PartitionError::InternalError);
        }
    }

    // ---- 3. Two-pointer partition: part=0 to the left, part=1 to the right ----
    int32_t l = first;
    int32_t r = first + N - 1;

    while (l <= r) {
        while (l <= r && localPart[sortedTo[sortedTriangles[l]] - first] == 0) ++l;
        while (l <= r && localPart[sortedTo[sortedTriangles[r]] - first] == 1) --r;
        if (l < r) {
            std::swap(sortedTriangles[l], sortedTriangles[r]);
            ++l;
            --r;
        }
    }

    int32_t leftN = l - first;

    // Rebuild sortedTo from the rearranged sortedTriangles
    for (int32_t pos = first; pos < first + N; ++pos) {
        sortedTo[sortedTriangles[pos]] = pos;
    }

    // ---- 4. Build left + right subgraphs ----
    auto BuildSubgraph = [&](int32_t side) -> MetisGraphWrapper::Result {
        // oldToNew: old local index → new local index 0..sideN-1
        std::vector<int32_t> oldToNew(N, -1);
        int32_t sideN = 0;
        for (int32_t v = 0; v < N; ++v) {
            if (localPart[v] == side) {
                oldToNew[v] = sideN++;
            }
        }

        MetisGraphWrapper builder(sideN);
        for (int32_t v = 0; v < N; ++v) {
            if (localPart[v] != side) continue;
            int32_t newV = oldToNew[v];
            for (idx_t e = graph.xadj[v]; e < graph.xadj[v + 1]; ++e) {
                int32_t neighbor = graph.adjncy[e];
                if (localPart[neighbor] == side) {
                    builder.AddEdge(newV, oldToNew[neighbor], graph.adjwgt[e]);
                }
            }
        }
        return builder.Build();
    };

    MetisGraphWrapper::Result leftSubgraph  = BuildSubgraph(0);
    MetisGraphWrapper::Result rightSubgraph = BuildSubgraph(1);

    // ---- 5. Recurse ----
    auto leftResult = Bisect(leftSubgraph, first, settings,
                             sortedTo, sortedTriangles, part, clusters);
    if (!leftResult) return std::unexpected(leftResult.error());

    auto rightResult = Bisect(rightSubgraph, first + leftN, settings,
                              sortedTo, sortedTriangles, part, clusters);
    if (!rightResult) return std::unexpected(rightResult.error());
    return {};
}

// ---- Public ----

std::expected<GraphPartitioner::Result, PartitionError>
GraphPartitioner::Partition(const MetisGraphWrapper::Result& graph,
                            const PartitionSettings& settings) {
    // ---- 1. Validate ----
    if (settings.minClusterSize > settings.maxClusterSize ||
        settings.maxClusterSize <= 0 ||
        graph.numVertices <= 0) {
        return std::unexpected(PartitionError::InvalidInput);
    }

    // ---- 2. Init identity mappings ----
    std::vector<int32_t> sortedTo(graph.numVertices);       // sortedTo[oldIdx] = newPos
    std::vector<int32_t> sortedTriangles(graph.numVertices); // sortedTriangles[pos] = oldIdx
    std::iota(sortedTo.begin(), sortedTo.end(), 0);
    std::iota(sortedTriangles.begin(), sortedTriangles.end(), 0);

    std::vector<int32_t> part(graph.numVertices);
    std::vector<Range> clusters;

    // ---- 3. Recursive bisection ----
    auto BisectRes = Bisect(graph, 0, settings, sortedTo, sortedTriangles, part, clusters);
    if (!BisectRes) return std::unexpected(BisectRes.error());

    // ---- 4. Return ----
    Result result;
    result.part      = std::move(part);
    result.clusters  = std::move(clusters);
    result.sortedTo  = std::move(sortedTo);
    return result;
}

} // namespace Tumbler::Nanite
