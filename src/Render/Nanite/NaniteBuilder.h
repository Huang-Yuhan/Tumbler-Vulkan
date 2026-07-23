#pragma once

#include <expected>

#include "Assets/MeshLoader.h"
#include "Render/Nanite/NaniteData.h"
#include "Render/Nanite/ClusterDag.h"

namespace Tumbler::Nanite {

enum class BuildError {
	EmptyMesh,
};

class NaniteBuilder {
public:
	// Build Nanite representation from a single mesh.
	// No serialization — pure in-memory processing.
	std::expected<NaniteData, BuildError> Build(const MeshData& mesh);
};

} // namespace Tumbler::Nanite
