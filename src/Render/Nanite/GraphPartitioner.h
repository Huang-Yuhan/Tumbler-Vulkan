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

// Partitions a mesh triangle graph into clusters using METIS.
//
// Partition() calls METIS_PartGraphRecursive to recursively
// bisect the graph into roughly equal-sized parts (~128 triangles each).
// Triangles within each cluster are sorted contiguous for GPU upload.
class GraphPartitioner {
public:
    struct Result {
        std::vector<int32_t> part;       // part[triIdx] = cluster id
        std::vector<Range>   clusters;    // clusters[c] = contiguous triangle range
        std::vector<int32_t> sortedTo;    // sortedTo[triIdx] = new position after sort
    };

    // numClusters = ceil(numTriangles / kClusterTriangleCount)
    std::expected<Result, PartitionError> Partition(
        const MetisGraphWrapper::Result& graph,
        int32_t numClusters);
};

} // namespace Tumbler::Nanite
