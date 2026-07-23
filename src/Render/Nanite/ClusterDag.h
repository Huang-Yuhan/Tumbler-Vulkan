#pragma once

#include <cstdint>
#include <vector>

#include "Assets/MeshLoader.h"
#include "Render/Nanite/Cluster.h"

namespace Tumbler::Nanite {

// Directed Acyclic Graph of Clusters across LOD levels.
//
//   LOD 0 (coarsest):   [C0] ─────────────┐
//                          │               │
//   LOD 1:           [C1] [C2]         [C3]
//                      │    │             │
//   LOD 2 (finest):  clusterlets...
//
// Edges go parent → child (from coarser to finer LOD).
// A parent Cluster can have multiple child Clusterlets.
// A child Clusterlet can have multiple parent Clusters (DAG, not tree).

class ClusterDag {
public:
	// Fields to be filled by you

	void AddMesh(const MeshData& data);

private:
	//图关系



};

} // namespace Tumbler::Nanite
