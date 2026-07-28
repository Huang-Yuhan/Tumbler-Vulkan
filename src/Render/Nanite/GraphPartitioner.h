#pragma once

#include "MetisGraphWrapper.h"
#include "Core/Utils/Range.h"

#include <cstdint>
#include <expected>
#include <vector>

namespace Tumbler::Nanite {

using Range = Tumbler::Utils::Range;

enum class PartitionError {
    InvalidInput,   // METIS_ERROR_INPUT
    OutOfMemory,    // METIS_ERROR_MEMORY
    InternalError,  // METIS_ERROR
};

struct PartitionSettings {
    int32_t minClusterSize = 64;   // triangles: floor of kClusterTriangleCount / 2
    int32_t maxClusterSize = 128;  // triangles: kClusterTriangleCount
};

// Partitions a mesh triangle graph into clusters using recursive METIS bisection.
//
// Partition() calls the recursive Bisect() helper on successive subgraphs.
// sortedTo maps each original triangle index to its position in the sorted
// output, where each cluster's triangles are contiguous.
class GraphPartitioner {
public:
    struct Result {
        std::vector<int32_t> part;       // part[triIdx] = cluster id
        std::vector<Range>   clusters;    // clusters[c] = contiguous triangle range
        std::vector<int32_t> sortedTo;    // sortedTo[triIdx] = new position after sort
    };

    std::expected<Result, PartitionError> Partition(
        const MetisGraphWrapper::Result& graph,
        const PartitionSettings& settings = {});

private:
    // Recursively bisect the subgraph at positions [first .. first+numVertices-1].
    //
    // sortedTriangles[pos] = global triangle index at position pos.
    // sortedTo[globalIdx]  = position of that triangle (inverse of sortedTriangles).
    //
    // graph uses local vertex indices 0..numVertices-1; local index i
    // corresponds to sortedTriangles[first + i].
    std::expected<void, PartitionError> Bisect(
        const MetisGraphWrapper::Result& graph,
        int32_t first,
        const PartitionSettings& settings,
        std::vector<int32_t>& sortedTo,         // sortedTo[oldIdx] = newPos
        std::vector<int32_t>& sortedTriangles,  // sortedTriangles[pos] = oldIdx
        std::vector<int32_t>& part,
        std::vector<Range>& clusters);
};

} // namespace Tumbler::Nanite
