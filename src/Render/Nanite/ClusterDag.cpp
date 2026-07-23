#include "ClusterDag.h"
#include "HalfEdge.h"
#include "NaniteHelper.h"

namespace Tumbler::Nanite {

// TODO: DAG construction and traversal

void ClusterDag::AddMesh(const MeshData& data) {

	auto& verts = data.vertices;
    auto& indices = data.indices;

	HalfEdgeHashTable hashTable;
    Adjacency adjacency(indices.size());

	auto GetPos = [&](size_t index) { return verts[indices[index]].pos; };

	for (size_t i = 0; i < indices.size(); i++)
	{
		hashTable.AddEdge(i, GetPos);
	}

	//每一个对边所在的三角形和它的twin所在的三角形一定是相邻的，应该在拓扑关系上有一条边

	for (size_t i = 0; i < indices.size(); i++)
	{
            hashTable.ForAllMatching(i, GetPos, [&](uint32_t i, uint32_t j) { 
				uint32_t target = std::numeric_limits<uint32_t>::max();
                adjacency.adj[i] = target;
            });
	}

}

} // namespace Tumbler::Nanite
