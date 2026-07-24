#pragma once

#include <metis.h>
#include <vector>

#include "Core/Utils/Range.h"

namespace Tumbler::Nanite {

	using Range = Tumbler::Utils::Range;

class GraphPartitioner {
	// Fields to be filled by you
public:
	struct GraphData
	{
        uint32_t Offset;
        uint32_t Num;

		std::vector<idx_t> Adjacency;
        std::vector<idx_t> AdjacencyCost;
		std::vector<idx_t> AdjacencyOffset;
	};

	// 这里没有一个GraphData的字段，是因为后续做图划分的时候会有多个Graph，每个GraphData对应一个图划分的子图，并不只有一个图

	std::vector<Range> Ranges; // 每个Range对应一个子图的顶点范围
        std::vector<uint32_t> Indexes;
        std::vector<uint32_t> SortedTo;

	GraphPartitioner(uint32_t numVertices, uint32_t MinPartitionSize, uint32_t MaxPartitionSize);

	GraphData* NewGraph(uint32_t NumAdjacency);
        void AddAdjacency(GraphData* graph, uint32_t AdjIndex, idx_t Cost);
        void AddLocalityLinks(GraphData* graph, uint32_t AdjIndex, idx_t Cost);
};

} // namespace Tumbler::Nanite
