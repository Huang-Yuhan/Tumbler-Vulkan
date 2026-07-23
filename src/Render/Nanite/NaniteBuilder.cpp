#include "NaniteBuilder.h"

namespace Tumbler::Nanite {

std::expected<NaniteData, BuildError> NaniteBuilder::Build(const MeshData& mesh) {
	if (mesh.vertices.empty() || mesh.indices.empty()) {
		return std::unexpected(BuildError::EmptyMesh);
	}

	// 我们先默认没有SubMesh的情况，后续再考虑SubMesh的情况

	//首先我们应该要根据Mesh去建立图关系
        NaniteData data;
        data.clusterDag.AddMesh(mesh);
        return data;
}

} // namespace Tumbler::Nanite
