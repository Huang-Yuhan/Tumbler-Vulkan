#include "GraphPartitioner.h"

#include <cassert>
#include <algorithm>

namespace Tumbler::Nanite {

void GraphPartitioner::BisectGraph(FGraphData* graph, FGraphData* childGraphs[2]) {
    childGraphs[0] = nullptr;
    childGraphs[1] = nullptr;

    auto AddPartition = [this](int32_t offset, int32_t num) {
        Range& range = Ranges[m_NumPartitions++];
        range.start  = offset;
        range.end    = offset + num;
    };

    if (graph->Num <= m_MaxPartitionSize) {
        AddPartition(graph->Offset, graph->Num);
        return;
    }

    const int32_t targetPartitionSize = (m_MinPartitionSize + m_MaxPartitionSize) / 2;
    const int32_t targetNumPartitions = std::max(2, (graph->Num + targetPartitionSize - 1) / targetPartitionSize);

    assert(graph->AdjacencyOffset.size() == static_cast<size_t>(graph->Num + 1));

    idx_t ncon  = 1;
    idx_t nparts = 2;
    idx_t edgecut = 0;

    real_t partitionWeights[] = {
        static_cast<float>(targetNumPartitions / 2) / targetNumPartitions,
        1.0f - static_cast<float>(targetNumPartitions / 2) / targetNumPartitions
    };

    idx_t options[METIS_NOPTIONS];
    METIS_SetDefaultOptions(options);

    // Allow looser imbalance tolerance at higher levels
    bool bLoose = targetNumPartitions >= 128 || m_MaxPartitionSize / m_MinPartitionSize > 1;
    options[METIS_OPTION_UFACTOR] = bLoose ? 200 : 1;

    int r = METIS_PartGraphRecursive(
        &graph->Num,
        &ncon,
        graph->AdjacencyOffset.data(),
        graph->Adjacency.data(),
        nullptr,                         // vwgt
        nullptr,                         // vsize
        graph->AdjacencyCost.data(),     // adjwgt
        &nparts,
        partitionWeights,                // tpwgts (imbalanced split)
        nullptr,                         // ubvec
        options,
        &edgecut,
        m_PartitionIDs.data() + graph->Offset
    );

    assert(r == METIS_OK);

    // ---- In-place two-pointer partition ----
    {
        int32_t front = graph->Offset;
        int32_t back  = graph->Offset + graph->Num - 1;
        while (front <= back) {
            while (front <= back && m_PartitionIDs[front] == 0) {
                m_SwappedWith[front] = front;
                ++front;
            }
            while (front <= back && m_PartitionIDs[back] == 1) {
                m_SwappedWith[back] = back;
                --back;
            }
            if (front < back) {
                std::swap(Indexes[front], Indexes[back]);
                m_SwappedWith[front] = back;
                m_SwappedWith[back]  = front;
                ++front;
                --back;
            }
        }

        int32_t split = front;
        int32_t num[2];
        num[0] = split - graph->Offset;
        num[1] = graph->Offset + graph->Num - split;

        assert(num[0] > 0);
        assert(num[1] > 0);

        if (num[0] <= m_MaxPartitionSize && num[1] <= m_MaxPartitionSize) {
            AddPartition(graph->Offset, num[0]);
            AddPartition(split,          num[1]);
        } else {
            for (int32_t i = 0; i < 2; ++i) {
                childGraphs[i]            = new FGraphData;
                childGraphs[i]->Adjacency.reserve(graph->Adjacency.size() >> 1);
                childGraphs[i]->AdjacencyCost.reserve(graph->Adjacency.size() >> 1);
                childGraphs[i]->AdjacencyOffset.reserve(num[i] + 1);
                childGraphs[i]->Num = num[i];
            }
            childGraphs[0]->Offset = graph->Offset;
            childGraphs[1]->Offset = split;

            for (int32_t i = 0; i < graph->Num; ++i) {
                FGraphData* child = childGraphs[i >= childGraphs[0]->Num ? 1 : 0];
                child->AdjacencyOffset.push_back(static_cast<idx_t>(child->Adjacency.size()));

                int32_t orgIndex = m_SwappedWith[graph->Offset + i] - graph->Offset;
                for (idx_t adjIdx = graph->AdjacencyOffset[orgIndex];
                     adjIdx < graph->AdjacencyOffset[orgIndex + 1]; ++adjIdx) {
                    idx_t adj     = graph->Adjacency[adjIdx];
                    idx_t adjCost = graph->AdjacencyCost[adjIdx];

                    adj = m_SwappedWith[graph->Offset + adj] - child->Offset;
                    if (0 <= adj && adj < child->Num) {
                        child->Adjacency.push_back(adj);
                        child->AdjacencyCost.push_back(adjCost);
                    }
                }
            }
            childGraphs[0]->AdjacencyOffset.push_back(
                static_cast<idx_t>(childGraphs[0]->Adjacency.size()));
            childGraphs[1]->AdjacencyOffset.push_back(
                static_cast<idx_t>(childGraphs[1]->Adjacency.size()));
        }
    }
}

void GraphPartitioner::RecursiveBisectGraph(FGraphData* graph) {
    FGraphData* childGraphs[2];
    BisectGraph(graph, childGraphs);
    delete graph;

    if (childGraphs[0] && childGraphs[1]) {
        RecursiveBisectGraph(childGraphs[0]);
        RecursiveBisectGraph(childGraphs[1]);
    }
}

void GraphPartitioner::PartitionStrict(FGraphData* graph, bool /*bThreaded*/) {
    m_PartitionIDs.resize(m_NumElements);
    m_SwappedWith.resize(m_NumElements);

    int32_t numPartitionsExpected = (graph->Num + m_MinPartitionSize - 1) / m_MinPartitionSize;
    Ranges.resize(numPartitionsExpected * 2);
    m_NumPartitions = 0;

    RecursiveBisectGraph(graph);

    Ranges.resize(m_NumPartitions);
    std::sort(Ranges.begin(), Ranges.end());

    m_PartitionIDs.clear();
    m_SwappedWith.clear();

    for (uint32_t i = 0; i < m_NumElements; ++i)
        SortedTo[Indexes[i]] = i;
}

} // namespace Tumbler::Nanite
