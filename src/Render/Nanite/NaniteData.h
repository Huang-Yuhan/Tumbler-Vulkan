#pragma once

#include "Render/Nanite/ClusterDag.h"

namespace Tumbler::Nanite {

// Output of NaniteBuilder::Build().
// Holds all the data needed for GPU-driven Nanite rendering.
struct NaniteData {
	// Fields to be filled by you
	ClusterDag clusterDag; // Directed Acyclic Graph of Clusters across LOD levels.
};

} // namespace Tumbler::Nanite
